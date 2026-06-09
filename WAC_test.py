import chess
import chess.engine

print("Booting up the Test...")

engine = chess.engine.SimpleEngine.popen_uci("./Mark_6")

correct = 0
total = 0

try:
    with open("wac.epd", "r") as f:
        for line in f:
            # python-chess EPD parser
            board, ops = chess.Board.from_epd(line)
            
            if "bm" not in ops:
                continue # skip if no best move is defined
            
            total += 1
            best_moves = ops["bm"]
            
            # give 1 second to think
            result = engine.play(board, chess.engine.Limit(time=1.0))
            
            if result.move in best_moves:
                correct += 1
                print(f"[{correct}/{total}] PASS | Found: {result.move}")
            else:
                print(f"[{correct}/{total}] FAIL | Played: {result.move} | Expected: {best_moves[0]}")

    print(f"\nFinal Tactical Score: {correct} / {total} ({(correct/total)*100:.1f}%)")

except FileNotFoundError:
    print("Error: Could not find 'wac.epd'.")

finally:
    engine.quit()