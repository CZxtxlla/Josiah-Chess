import subprocess
import os
import multiprocessing

# Configure
ENGINE_PATH = "./Mark_10NNUE768_v6_50_pure" # compiled binary
NUM_WORKERS = 8
GAMES_PER_WORKER = 5000
OUTPUT_DIR = "selfplay_data"


def run_engine_worker(worker_id):

    output_file = f"{OUTPUT_DIR}/training_data_thread_{worker_id}.bin"
    print(f"Worker {worker_id} started. Targeting {GAMES_PER_WORKER} games in {output_file}")

    uci_commands = f"datagen {GAMES_PER_WORKER} {output_file}\nquit\n"

    process = subprocess.Popen([ENGINE_PATH], stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, text=True)

    process.communicate(input = uci_commands)

    print(f"Worker {worker_id} finished successfully")


if __name__ == "__main__":

    if not os.path.exists(OUTPUT_DIR):
        os.makedirs(OUTPUT_DIR)

    print(f"Starting datagen orchestrator...")
    print(f"Spawning {NUM_WORKERS} isolated engine processes...")
    print(f"Total games to generate: {NUM_WORKERS * GAMES_PER_WORKER}\n")
    
    with multiprocessing.Pool(NUM_WORKERS) as pool:
        pool.map(run_engine_worker, range(NUM_WORKERS))
        
    print("\nAll workers finished. Data generation complete")