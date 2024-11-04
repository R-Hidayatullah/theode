use std::{collections::HashMap, time::Instant};

use dat_parser::ArchiveId;
use pbr::ProgressBar;

mod dat_parser;

fn main() {
    let file_path = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Guild Wars 2\\Gw2.dat";
    // let file_path = "Local.dat";

    let mut gw2dat = dat_parser::DatFile::load_from_file(file_path.to_string()).unwrap();

    println!("MFT data size : {}", &gw2dat.mft_data.len());
    println!("MFT index data size : {}", &gw2dat.mft_index_data.len());

    // Create a HashMap to store base IDs and associated file IDs.
    let mut base_file_map: HashMap<u32, Vec<u32>> = HashMap::new();

    // Populate the HashMap with base IDs and file IDs.
    for index_data in &gw2dat.mft_index_data {
        base_file_map
            .entry(index_data.base_id)
            .or_insert_with(Vec::new)
            .push(index_data.file_id);
    }

    println!("Hashmap size : {}", &base_file_map.len());
    let mut progress = ProgressBar::new(base_file_map.len() as u64);
    progress.format("╢▌▌░╟");

    // Benchmark extraction and usage of base_file_map.
    let start = Instant::now();

    // Get the list of file IDs for base ID 16.
    // Iterate over each file ID in the result and retrieve the MFT data.
    for file_id in base_file_map.iter() {
        progress.inc();
        gw2dat
            .get_mft_data(
                file_path.to_string(),
                ArchiveId::FileId,
                file_id.1.get(0).unwrap().clone(),
            )
            .unwrap();
    }

    let duration = start.elapsed();
    progress.finish_print("Done!\n");

    println!(
        "Time taken for extraction and MFT data retrieval: {:?}",
        duration
    );
}
