import subprocess
import sys

def scp(file, dest):
    ch = "'"
    scp_command = f'scp -o {ch}ProxyJump alonbentzi@bava.cs.huji.ac.il{ch} {file} alonbentzi@river:{dest}'
    subprocess.run(scp_command, shell=True, check=True)

if __name__ == "__main__":
	scp(sys.argv[1], sys.argv[2])
