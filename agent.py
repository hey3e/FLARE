import os
import subprocess
import signal
import random
import time
import socket
from itertools import combinations

server_ip = '127.0.0.1'
server_port = 4433

cipher_suites = [
    'TLS_AES_128_GCM_SHA256',
    'TLS_AES_256_GCM_SHA384',
    'TLS_CHACHA20_POLY1305_SHA256',
    'TLS_AES_128_CCM_SHA256',
    'TLS_AES_128_CCM_8_SHA256'
]

#24755, dataflow-only; 36670, ours

start_point = 0
end_point = 36670
increased = set()

start_time = time.time()

coverage = 0
with open("path/to/coverage.log") as f:
    coverage = int(f.read())

for i in range(end_point):
    env = os.environ.copy()
    env["MUTATION_POINT"] = str(i)
    cmd = f"./apps/openssl s_client -connect 127.0.0.1:4433 -tls1_3 -cert cert.pem -key key.pem | grep '\[+\]'"
    process = subprocess.Popen(cmd, shell=True, env=env, preexec_fn=os.setsid)
    try:
        process.wait(timeout=0.1)
    except subprocess.TimeoutExpired:
        os.killpg(os.getpgid(process.pid), signal.SIGTERM)

    time.sleep(0.3)

    with open("/path/to/coverage.log") as f:
        current_coverage = int(f.read())

    if current_coverage > coverage:
        increased.add(i)
        print("[+]Found new path.\n")
        coverage = current_coverage

for i in increased:
    for j in range(10):
        env = os.environ.copy()
        env["MUTATION_POINT"] = str(i)

        port = 4433 
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        result = sock.connect_ex(('127.0.0.1', port))
        if result != 0:
            print("[+]Server unreachable,, skipping")
            break
        sock.close()

        cmd = f"./apps/openssl s_client -connect 127.0.0.1:4433 -tls1_3 -cert cert.pem -key key.pem | grep '\[+\]'"
        process = subprocess.Popen(cmd, shell=True, env=env, preexec_fn=os.setsid)

        try:
            process.wait(timeout=0.2)
        except subprocess.TimeoutExpired:
            os.killpg(os.getpgid(process.pid), signal.SIGTERM)

        time.sleep(0.1)

while True:
    length = random.randint(2, len(increased))
    combo = random.sample(increased, k=length)
    points_str = ",".join(map(str, combo))

    env = os.environ.copy()
    env["MUTATION_POINT"] = points_str
    print("[+]Mutating... " + points_str + "\n")

    port = 4433 
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    result = sock.connect_ex(('127.0.0.1', port))
    if result != 0:
        print("[+]Server unreachable,, skipping")
        break
    sock.close()

    random_suite = random.choice(cipher_suites)
    cmd = f"./apps/openssl s_client -connect 127.0.0.1:4433 -tls1_3 -cert cert.pem -key key.pem | grep '\[+\]'"
    process = subprocess.Popen(cmd, shell=True, env=env, preexec_fn=os.setsid)
    try:
        process.wait(timeout=0.1)
    except subprocess.TimeoutExpired:
        os.killpg(os.getpgid(process.pid), signal.SIGTERM)

end_time = time.time()
total_time = end_time - start_time

print(f"Total time: {total_time} seconds")