import subprocess
import sys
import time

ATSHOGI_PATH = r"C:\VS\Workspace\atshogi-android\build\atshogi_engine_host.exe"
SUISHO_PATH = r"C:\Suisho5-YaneuraOu-v7.5.0-windows\YaneuraOu_NNUE-tournament-clang++-zen2.exe"

class UsiEngine:
    def __init__(self, name, exe_path, env_path=None):
        self.name = name
        import os
        env = os.environ.copy()
        if env_path:
            env["PATH"] = env_path + ";" + env["PATH"]
            
        self.proc = subprocess.Popen(
            [exe_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            env=env
        )

    def send(self, cmd):
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()

    def read_line(self):
        line = self.proc.stdout.readline()
        if not line:
            return None
        return line.strip()

    def wait_prefix(self, prefix, timeout=10):
        start = time.time()
        while time.time() - start < timeout:
            line = self.read_line()
            if line is None:
                raise RuntimeError(f"Engine {self.name} died")
            if line.startswith("bestmove"):
                print(f"[{self.name}] {line}")
            if self.name == "ATShogi" and not line.startswith("bestmove") and not line.startswith("info"):
                print(f"[ATShogi] {line}")
            if line.startswith(prefix):
                return line
        raise TimeoutError(f"Engine {self.name} timed out")

    def quit(self):
        try:
            self.send("quit")
            self.proc.wait(timeout=2)
        except Exception:
            try:
                self.proc.kill()
            except Exception:
                pass


def print_board(moves):
    # Minimal board representation to show end state, or we can just print the Kifu
    import cshogi
    board = cshogi.Board()
    for move in moves:
        board.push_usi(move)
    print(board)


def run_match(atshogi_is_sente, max_ply=150):
    print(f"=== MATCH START (ATShogi Sente: {atshogi_is_sente}) ===")
    ghc_path = r"C:\ghcup\ghc\9.10.3\mingw\bin"
    at = UsiEngine("ATShogi", ATSHOGI_PATH, env_path=ghc_path)
    su = UsiEngine("Suisho", SUISHO_PATH)

    try:
        at.send("usi")
        at.wait_prefix("usiok")
        at.send("isready")
        at.wait_prefix("readyok")
        at.send("usinewgame")

        su.send("usi")
        su.wait_prefix("usiok")
        su.send("setoption name SkillLevel value 0")
        su.send("isready")
        su.wait_prefix("readyok")
        su.send("usinewgame")

        moves = []
        result = "maxply"

        for ply in range(1, max_ply + 1):
            is_sente = (ply % 2 == 1)
            is_at_turn = (is_sente == atshogi_is_sente)
            actor = at if is_at_turn else su

            pos_cmd = "position startpos" if not moves else f"position startpos moves {' '.join(moves)}"
            actor.send(pos_cmd)
            
            if is_at_turn:
                actor.send("go movetime 1000")
            else:
                actor.send("go depth 1") # shallow depth to make it play fast

            best_line = actor.wait_prefix("bestmove", timeout=60)
            tokens = best_line.split()
            bestmove = tokens[1] if len(tokens) >= 2 else "resign"

            if bestmove in ("resign", "resign\n", "win", "none"):
                winner = "ATShogi" if not is_at_turn else "Suisho"
                if bestmove == "win":
                    winner = "ATShogi" if is_at_turn else "Suisho"
                result = f"Winner: {winner} ({bestmove})"
                break

            moves.append(bestmove)
            
            # Simple mate check
            import cshogi
            b = cshogi.Board()
            for m in moves:
                b.push_usi(m)
            if b.is_game_over():
                result = "Checkmate or Draw"
                break

        print(f"=== RESULT: {result} ===")
        print(f"Kifu: {' '.join(moves)}")
        print("Final Board:")
        print_board(moves)
        print("=============================\n")

    finally:
        at.quit()
        su.quit()

if __name__ == "__main__":
    import cshogi
    print("Testing ATShogi vs Suisho (2 games)")
    run_match(atshogi_is_sente=True)
    run_match(atshogi_is_sente=False)
