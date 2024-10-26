use dat_parser::ArchiveId;

mod dat_decompress;
mod dat_parser;

fn main() {
    let mut gw2dat = dat_parser::DatFile::load_from_file("Local.dat").unwrap();
    println!("MFT data size : {}", &gw2dat.mft_data.len());
    println!("MFT index data size : {}", &gw2dat.mft_index_data.len());

    let data = gw2dat
        .get_mft_data("Local.dat", ArchiveId::FileId, 2)
        .unwrap();
    dat_parser::DatFile::print_first_16_bytes(&data);
    println!("Hi");
}