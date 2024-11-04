use std::{
    fs::File,
    io::{self, BufReader, Cursor, Read, Seek, SeekFrom},
    path::Path,
};

use byteorder::{LittleEndian, ReadBytesExt};
use serde::{Deserialize, Serialize};

#[link(name = "dat_decompress")] // Ensure this links to the correct C library
extern "C" {
    pub fn inflate_buffer(
        input_buffer_size: u32,         // Corresponds to uint32_t input_buffer_size
        input_buffer: *const u8,        // Corresponds to uint32_t *input_buffer
        output_buffer_size: *mut u32,   // Corresponds to uint32_t *output_buffer_size
        custom_output_buffer_size: u32, // Corresponds to uint32_t custom_output_buffer_size
    ) -> *mut u8; // Corresponds to uint8_t *
}

#[derive(Default, Debug, Serialize, Deserialize)]
pub struct DatHeader {
    version: u8,
    identifier: Vec<u8>,
    header_size: u32,
    unknown_field: u32,
    chunk_size: u32,
    crc: u32,
    unknown_field_2: u32,
    mft_offset: u64,
    mft_size: u32,
    flags: u32,
}

#[derive(Default, Debug, Serialize, Deserialize)]
pub struct MFTHeader {
    identifier: Vec<u8>,
    unknown: u64,
    num_entries: u32,
    unknown_field_2: u32,
    unknown_field_3: u32,
}

#[derive(Default, Debug, Serialize, Deserialize)]
pub struct MFTData {
    pub offset: u64,
    pub size: u32,
    pub compression_flag: u16,
    pub entry_flag: u16,
    pub counter: u32,
    pub crc: u32,
}

#[derive(Default, Debug, Serialize, Deserialize)]
pub struct MFTIndexData {
    pub file_id: u32,
    pub base_id: u32,
}

#[derive(Default, Debug, Serialize, Deserialize)]
pub struct DatFile {
    header: DatHeader,
    mft_header: MFTHeader,
    pub mft_data: Vec<MFTData>,
    pub mft_index_data: Vec<MFTIndexData>,
}

#[derive(Debug, Serialize, Deserialize)]
pub enum ArchiveId {
    FileId,
    BaseId,
}

const DAT_MAGIC_NUMBER: usize = 3;
const MFT_MAGIC_NUMBER: usize = 4;
const MFT_ENTRY_INDEX_NUM: usize = 1;

impl DatFile {
    pub fn load_from_file<P: AsRef<Path>>(file_path: P) -> io::Result<Self> {
        // Check if the file extension is '.dat'
        let file_path_str = file_path.as_ref().to_str().unwrap_or("");
        if !file_path_str.to_lowercase().ends_with(".dat") {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "Invalid file extension. Expected '.dat'.",
            ));
        }

        // Open the file and create a buffered reader.
        let file = std::fs::File::open(file_path)?;
        let mut buf_reader = BufReader::new(file);

        // Delegate to load_from_reader for further processing.
        Self::load_from_reader(&mut buf_reader)
    }

    fn load_from_reader<R: Read + Seek>(reader: &mut R) -> io::Result<Self> {
        let mut dat_data = DatFile::default();
        dat_data.read_header(reader)?;
        dat_data.read_mft_header(reader)?;
        dat_data.read_mft_data(reader)?;
        dat_data.read_mft_index(reader)?;

        Ok(dat_data)
    }

    fn read_header<R: Read + Seek>(&mut self, file: &mut R) -> io::Result<&mut Self> {
        // Read version information
        self.header.version = file.read_u8()?;

        // Read magic number
        let mut magic = [0; DAT_MAGIC_NUMBER];
        file.read_exact(&mut magic)?;
        self.header.identifier = Vec::from(magic);

        self.header.header_size = file.read_u32::<LittleEndian>()?;
        self.header.unknown_field = file.read_u32::<LittleEndian>()?;
        self.header.chunk_size = file.read_u32::<LittleEndian>()?;
        self.header.crc = file.read_u32::<LittleEndian>()?;
        self.header.unknown_field_2 = file.read_u32::<LittleEndian>()?;
        self.header.mft_offset = file.read_u64::<LittleEndian>()?;
        self.header.mft_size = file.read_u32::<LittleEndian>()?;
        self.header.flags = file.read_u32::<LittleEndian>()?;

        // Check the magic number to verify if it's a valid DAT file
        // let check_magic = [0x41, 0x4E, 0x1A];
        // if self.header.identifier != check_magic {
        //     panic!("Not a DAT file: invalid header magic");
        // }
        println!("{:?}", self.header);
        Ok(self)
    }

    fn read_mft_header<R: Read + Seek>(&mut self, file: &mut R) -> io::Result<&mut Self> {
        file.seek(std::io::SeekFrom::Start(self.header.mft_offset as u64))?;
        // Read magic number
        let mut magic = [0; MFT_MAGIC_NUMBER];
        file.read_exact(&mut magic)?;
        self.mft_header.identifier = Vec::from(magic);

        self.mft_header.unknown = file.read_u64::<LittleEndian>()?;
        self.mft_header.num_entries = file.read_u32::<LittleEndian>()?;
        self.mft_header.unknown_field_2 = file.read_u32::<LittleEndian>()?;
        self.mft_header.unknown_field_3 = file.read_u32::<LittleEndian>()?;

        // Check the magic number to verify if it's a valid MFT file
        let check_magic = [0x4D, 0x66, 0x74, 0x1A];
        if self.mft_header.identifier != check_magic {
            panic!("Not a MFT file: invalid header magic");
        }
        println!("\n{:?}", self.mft_header);

        Ok(self)
    }

    fn read_mft_data<R: Read + Seek>(&mut self, file: &mut R) -> io::Result<&mut Self> {
        for _ in 0..(self.mft_header.num_entries) {
            self.mft_data.push(MFTData {
                offset: file.read_u64::<LittleEndian>()?,
                size: file.read_u32::<LittleEndian>()?,
                compression_flag: file.read_u16::<LittleEndian>()?,
                entry_flag: file.read_u16::<LittleEndian>()?,
                counter: file.read_u32::<LittleEndian>()?,
                crc: file.read_u32::<LittleEndian>()?,
            });
        }
        Ok(self)
    }
    fn read_mft_index<R: Read + Seek>(&mut self, file: &mut R) -> io::Result<&mut Self> {
        println!(
            "NUM FILE ID ENTRIES : {}",
            self.mft_data.get(MFT_ENTRY_INDEX_NUM).unwrap().size / 8
        );
        file.seek(std::io::SeekFrom::Start(
            self.mft_data.get(MFT_ENTRY_INDEX_NUM).unwrap().offset as u64,
        ))?;
        for _ in 0..(self.mft_data.get(MFT_ENTRY_INDEX_NUM).unwrap().size / 8) {
            self.mft_index_data.push(MFTIndexData {
                file_id: file.read_u32::<LittleEndian>()?,
                base_id: file.read_u32::<LittleEndian>()?,
            });
        }
        Ok(self)
    }

    pub fn get_mft_data<P: AsRef<Path>>(
        &mut self,
        file_path: P,
        number_type: ArchiveId, // Add this parameter
        number: u32,            // Add this parameter
    ) -> io::Result<Vec<u8>> {
        // Check if the file extension is '.dat'
        let file_path_str = file_path.as_ref().to_str().unwrap();
        if !file_path_str.to_lowercase().ends_with(".dat") {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "Invalid file extension. Expected '.dat'.",
            ));
        }

        // Check if the index is valid based on number_type
        let mft_index_entry = &self.mft_index_data;
        let mut num_index: usize = 0;
        match number_type {
            ArchiveId::FileId => {
                for i in 0..mft_index_entry.len() {
                    if mft_index_entry.get(i).unwrap().file_id == number {
                        println!("Found : \n{:?}\n", mft_index_entry.get(i));
                        num_index = mft_index_entry.get(i).unwrap().base_id as usize;
                    }
                }
            }
            ArchiveId::BaseId => {
                for i in 0..mft_index_entry.len() {
                    if mft_index_entry.get(i).unwrap().base_id == number {
                        println!("Found : \n{:?}\n", mft_index_entry.get(i));

                        num_index = mft_index_entry.get(i).unwrap().base_id as usize;
                    }
                }
            }
        };

        num_index = num_index - 1;

        println!(
            "Valid number {} : {:?}",
            num_index + 1,
            &self.mft_data.get(num_index)
        );

        // Open the file and create a buffered reader.
        let file = File::open(file_path)?;
        let mut buf_reader = BufReader::new(file);

        let mft_table = &self.mft_data[num_index];

        // Call mft_read_data to read the compressed data
        let data = Self::mft_read_data(&mut buf_reader, mft_table.offset, mft_table.size);

        DatFile::print_first_16_bytes(&data);

        if mft_table.compression_flag != 0 {
            println!("Compressed!");
            let input_buffer_size: u32 = data.len() as u32;
            let mut output_buffer_size: u32 = data.len() as u32;
            let custom_output_buffer_size: u32 = 0;

            // Call the external C function
            let result_ptr = unsafe {
                inflate_buffer(
                    input_buffer_size,
                    data.as_ptr(),
                    &mut output_buffer_size,
                    custom_output_buffer_size,
                )
            };

            // Check if the result pointer is null, indicating failure
            if result_ptr.is_null() {
                panic!("Decompression failed.");
            }

            if custom_output_buffer_size > 0 {
                output_buffer_size = custom_output_buffer_size;
            }

            // Convert the result pointer to a slice
            let output_slice =
                unsafe { std::slice::from_raw_parts(result_ptr, output_buffer_size as usize) };
            DatFile::print_first_16_bytes(&output_slice);
            Ok(output_slice.to_vec())
        } else {
            DatFile::print_first_16_bytes(&data);

            Ok(data)
        }
    }

    fn mft_read_data(file: &mut BufReader<File>, offset: u64, length: u32) -> Vec<u8> {
        file.seek(SeekFrom::Start(offset)).unwrap();
        let mut data = vec![0; length as usize];
        file.read_exact(&mut data).unwrap();
        data
    }

    pub fn print_first_16_bytes(data: &[u8]) {
        let length = data.len().min(16); // Ensure we only print up to 16 bytes

        println!("First {} bytes of data:", length);
        // Print in hexadecimal
        println!("Hex: ");
        for i in 0..length {
            print!("{:02X} ", data[i]);
        }
        println!();

        // Print in ASCII
        println!("ASCII: ");
        for i in 0..length {
            if data[i] > 31 && data[i] < 127 {
                print!("{}", data[i] as char);
            } else {
                print!(".");
            }
        }
        println!();
    }
}
