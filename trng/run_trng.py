import subprocess

def run_trng():
    result = subprocess.run(
        ["./trng/generator"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True
    )
    return result.stdout
