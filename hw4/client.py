import socket
import sys
import os
import argparse
from argparse import RawTextHelpFormatter

MAXLINE = 2048
input_cmd = []
output_cmd = []

def get_argments():
    parser = argparse.ArgumentParser()
    parser.add_argument("ip", type=str)
    parser.add_argument("port", type=int)
    # parser.add_argument("--port", "-p", "127.0.0.1", type=int, dest="port", help="connect to server port")
    args = parser.parse_args()

    return args

arg = get_argments()
# print(arg.ip)
# print(arg.port)
# Host = "127.0.0.1"
Host = arg.ip
Port = arg.port

cmd = "telnet " + Host + " " + str(Port)
os.system(cmd)

# with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_s:
#     server_s.connect((Host, Port))
#     msg = server_s.recv(MAXLINE).decode("utf-8")
#     print(str(msg), end="")
#     while 1:
#         input_cmd = input()
#         if len(input_cmd) == 0:
#             print("% ", end = "")
#             continue
#         sendmsg = input_cmd + "\r\n"
#         server_s.send(sendmsg.encode("utf-8"))
#         msg = server_s.recv(MAXLINE).decode("utf-8").strip(b'\x00'.decode())
#         if not msg:
#             break
#         else:
#             # output_cmd = func(input_cmd, str(msg), now_user)
#             print(msg, end="")
#     server_s.close()
