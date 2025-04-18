#!/usr/bin/env python3



import os

import subprocess

import signal

import time

import random



# 固定策略

os.environ["REPEAT_REMOVE"] = "1"



# 三台服务器端口和对应的日志文件

SERVER_PORTS = [4433, 4434, 4435]

PLUS_LOGS = {

    4433: "/path/to/plus4433.log",

    4434: "/path/to/plus4434.log",

    4435: "/path/to/plus4435.log"

}



# 客户端证书路径

CERT = "/path/to/client-cert.pem"

KEY  = "/path/to/client-key.pem"

CA   = "/path/to/ca-cert.pem"



# 记录上一次已读到的日志行数，用于增量读取

last_line_count = {

    4433: 0,

    4434: 0,

    4435: 0

}



def read_new_lines(path, old_count):

    """

    从 path 文件读取全部行，然后只返回

    old_count 之后的新行列表，以及新的行总数。

    """

    if not os.path.isfile(path):

        return [], 0  # 文件不存在，返回空



    with open(path, 'r', encoding='utf-8', errors='ignore') as f:

        all_lines = f.readlines()

    

    new_total = len(all_lines)

    # 只取 old_count..(new_total-1) 这些行

    if old_count < new_total:

        new_lines = all_lines[old_count:]

    else:

        new_lines = []



    return new_lines, new_total



def read_plus_log_incr(port):

    """

    从 plusXXX.log 中增量读取本轮新产生的行，去掉空行并去重。

    """

    path = PLUS_LOGS[port]

    global last_line_count



    new_lines, new_total = read_new_lines(path, last_line_count[port])

    last_line_count[port] = new_total  # 更新计数

    

    # 去重 + 去空行

    s = set()

    for line in new_lines:

        line_stripped = line.strip()

        if line_stripped:

            s.add(line_stripped)

    return s



def run_client_once(port, replace_id, envseed):

    """

    连接指定端口，设置 REPLACE=replace_id, ENVSEED=envseed

    """

    env = os.environ.copy()

    env["REPLACE"] = str(replace_id)

    env["ENVSEED"] = str(envseed)



    cmd = (

        f"./apps/openssl s_client "

        f"-connect 127.0.0.1:{port} "

        f"-tls1_3 "

        f"-cert {CERT} "

        f"-key {KEY} "

        f"-CAfile {CA}"

    )



    print(f"[*] Connecting port {port}, REPLACE={replace_id}, ENVSEED={envseed}")

    process = subprocess.Popen(

        cmd, shell=True, env=env,

        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,

        preexec_fn=os.setsid

    )



    try:

        outs, _ = process.communicate(timeout=1.0)

        for line in outs.decode('utf-8', errors='ignore').splitlines():

            if "[+]" in line:

                print(line)

    except subprocess.TimeoutExpired:

        os.killpg(os.getpgid(process.pid), signal.SIGTERM)

        print(f"[-] Timeout on port {port}")



def check_consistency():

    # 增量读本轮新行

    sets = {}

    for port in SERVER_PORTS:

        sets[port] = read_plus_log_incr(port)

        print(f"[+] {PLUS_LOGS[port]}: {len(sets[port])} new lines this round")



    # 基础比较

    base_port = SERVER_PORTS[0]

    base_set = sets[base_port]

    consistent = True



    for port in SERVER_PORTS[1:]:

        if sets[port] != base_set:

            consistent = False

            print(f"[!] Inconsistency between port {base_port} and {port}")

            only_base = base_set - sets[port]

            only_other = sets[port] - base_set

            if only_base:

                print(f"    Only in {base_port}:")

                for line in only_base:

                    print("      " + line)

            if only_other:

                print(f"    Only in {port}:")

                for line in only_other:

                    print("      " + line)



    if consistent:

        print("[✓] All server logs are consistent this round.\n")



def main_loop():

    print("[*] Fuzzing consistency test started...")



    # 如果想在脚本启动时清空日志，可以这里做:

    # for log in PLUS_LOGS.values():

    #     if os.path.exists(log):

    #         with open(log, "w"):

    #             pass



    # 每次random.seed(None)让系统自动选择随机种子

    random.seed()



    while True:

        for i in range(23):  # REPLACE = 0..22

            # 每轮生成一个随机ENVSEED

            envseed = random.randint(0, 65535)



            # 连接三台服务器, 同一轮使用同一个ENVSEED

            for port in SERVER_PORTS:

                run_client_once(port, i, envseed)

                time.sleep(0.3)



            # 等待server写日志

            time.sleep(1.0)



            # 一致性检查(只读本轮新增日志)

            check_consistency()



            # 等5秒再下一轮

            time.sleep(3.0)



if __name__ == "__main__":

    main_loop()

