#!/usr/bin/env python3

import boto3
import socket
import sys
import os
import re
import math
import argparse
from argparse import RawTextHelpFormatter

MAXLINE = 1024
input_cmd = []
output_cmd = []
article = {}
art_i = 0;
now_user = "UNKNOWN"

# use Amazon S3
s3 = boto3.resource('s3')

# Print out bucket names
# for bucket in s3.buckets.all():
#     print(bucket.name)

def get_argments():
    parser = argparse.ArgumentParser()
    parser.add_argument("ip", type=str)
    parser.add_argument("port", type=int)
    # parser.add_argument("--port", "-p", "127.0.0.1", type=int, dest="port", help="connect to server port")
    args = parser.parse_args()

    return args

def func(inputmsg, server_msg, nw_user):
    server_arg = server_msg.split()
    client_arg = input_cmd.split()
    relist = []
    # 0: msg, 1: now user name(default is UNKNOWN)
    relist.append(server_msg)
    relist.append(nw_user)
    if "Register successfully." in server_msg:
        register(client_arg)
        return relist
    elif "Welcome," in server_arg:
        user = login(client_arg)
        return relist
    elif "Create post successfully." in server_msg and "--content" in client_arg:
        relist[0] = creat_post(inputmsg, server_msg)
        return relist
    elif "read" in client_arg and "Post does not exist." not in server_msg:
        relist[0] = read(server_msg, client_arg[1])
        return relist
    elif "Comment successfully." in server_msg:
        relist[0] = comment(inputmsg, server_msg)
        return relist
    elif "Update successfully." in server_msg and "--content" in inputmsg:
        relist[0] = update_post(inputmsg, server_msg)
        return relist
    elif "Update successfully." in server_msg and "--title" in inputmsg:
        tmp = server_msg.split("#")
        relist[0] = tmp[1]
        return relist
    elif "Delete successfully." in server_msg:
        relist[0] = delete_post(inputmsg, server_msg)
        return relist
    elif "Sent successfully." in server_msg:
        relist[0] = mailto(inputmsg, server_msg)
        return relist
    elif "list-mail" in inputmsg and "Please login first." not in server_msg:
        relist[0] = "% "
        listmail(server_msg)
        return relist
    elif "retr-mail" in inputmsg and "Please login first." not in server_msg and "No such mail." not in server_msg:
        relist[0] = "% "
        retrmail(server_msg)
        return relist
    elif "delete-mail" in inputmsg and "Please login first." not in server_msg and "No such mail." not in server_msg:
        relist[0] = "Mail deleted.\n% "
        deletemail(server_msg)
        return relist
    else:
        return relist

def register(client_arg):
    # print("[$client]: register function called")
    s3.create_bucket(Bucket=("nphw0616037userx"+client_arg[1].lower()))

def login(client_arg):
    # print("[$client]: login function called")
    user = client_arg[1]
    return user

def creat_post(inputmsg, server_msg):
    # print("[$client]: creat_post function called")
    server_arg = server_msg.split("#", 2)
    client_arg = inputmsg.split("--content ")
    postid = server_arg[0]
    user = server_arg[1]
    # print("--->" + postid + ", " + user)
    content = client_arg[1]
    con = content.replace("<br>", "\n") + "\n--"
    # print("-------------------------------")
    # print(con)
    # print("-------------------------------")
    filename = postid + ".txt"
    awsfilepath = "post/" + filename
    fp = open(filename, "w")
    fp.write(con)
    fp.close()
    # print("Create PostId is -->" + postid)
    # print("nphw0616037userx"+user)
    # print(filename)
    # print(awsfilepath)
    target_bucket = s3.Bucket("nphw0616037userx"+user.lower())
    target_bucket.upload_file(filename, awsfilepath)
    return server_arg[2]

def read(server_msg, postid):
    postname = postid + ".txt"
    postpath = "post/" + postname
    # print("[$client]: read function called")
    content_arg = server_msg.split("--\n", 1)
    # print(content_arg[1])
    data_arg = re.split('[:\n]', content_arg[0])
    user = data_arg[1]
    # print(user)
    # output = content_arg[0]
    target_bucket = s3.Bucket("nphw0616037userx"+user.lower())
    target_object = target_bucket.Object(postpath)
    content = target_object.get()["Body"].read().decode() #get the content of hello.txt
    # print("-------------------------------")
    # print(content)
    # print("-------------------------------")
    output = content_arg[0] + "--\n" + content + "\n% "
    return output

def comment(inputmsg, server_msg):
    # print("[$client]: comment function called")
    com_arg = inputmsg.split(" ", 2)
    output = server_msg.split("#", 2)
    user = output[0]
    post_owner = output[1]
    postid = com_arg[1]
    # print(user + ", " + post_owner + ", " + postid)
    # print("--"+com_arg[2]+"--")
    postname = postid + ".txt"
    postpath = "post/" + postname
    fp = open(postname, "a")
    fp.write("\n"+user+": "+com_arg[2])
    fp.close()
    target_bucket = s3.Bucket("nphw0616037userx"+post_owner.lower())
    target_bucket.upload_file(postname, postpath)

    # target_object = target_bucket.Object(postpath)
    # content = target_object.get()["Body"].read().decode() #get the content of hello.txt
    # print("-------------------------------")
    # print(content)
    # print("-------------------------------")

    return output[2]

def update_post(inputmsg, server_msg):
    # print("[$client]: update function called")
    output = server_msg.split("#", 1)
    update_arg = inputmsg.split(" ", 3)
    user = output[0]
    postid = update_arg[1]
    update_value = update_arg[3]
    postname = postid + ".txt"
    postpath = "post/" + postname
    target_bucket = s3.Bucket("nphw0616037userx"+user.lower())
    target_object = target_bucket.Object(postpath)
    content = target_object.get()["Body"].read().decode() #get the content of hello.txt
    # print("-------------------------------")
    # print(content)
    # print("-------------------------------")
    only_content = content.split("--")
    only_content[0] = update_value.replace("<br>", "\n")
    updated = "\n--".join(only_content)
    # print("-------------------------------")
    # print(updated)
    # print("-------------------------------")
    fp = open(postname, "w")
    fp.write(updated)
    fp.close()
    target_bucket.upload_file(postname, postpath)

    return output[1]

def delete_post(inputmsg, server_msg):
    # print("[$client]: delete function called")
    client_arg = inputmsg.split(" ", 1)
    server_arg = server_msg.split("#", 1)
    postid = client_arg[1]
    user = server_arg[0]
    # print(user+", "+postid)
    postname = postid + ".txt"
    postpath = "post/" + postname
    # print("nphw0616037userx"+user)
    # print(postname)
    # print(postpath)
    target_bucket = s3.Bucket("nphw0616037userx"+user.lower())
    target_object = target_bucket.Object(postpath)
    target_object.delete()
    return server_arg[1]

def mailto(inputmsg, server_msg):
    # print("[$client]: mail-to function called")
    client_arg = inputmsg.split(" ", 5)
    tmp = inputmsg.split(" --content ", 1)
    ttmp = tmp[0].split("--subject ", 1)
    server_arg = server_msg.split("#", 1)
    user = client_arg[1]
    mailname = ttmp[1]
    content = tmp[1]
    mailid = server_arg[0]
    con = content.replace("<br>", "\n")
    # print(user+", "+postid)
    filename = mailid + "mail.txt"
    filepath = "mailbox/" + filename
    fp = open(filename, "w")
    fp.write(con)
    fp.close()
    # print("nphw0616037userx"+user)
    # print(filename)
    # print(filepath)
    target_bucket = s3.Bucket("nphw0616037userx"+user.lower())
    target_bucket.upload_file(filename, filepath)
    return server_arg[1]

def listmail(server_msg):
    # print("[$client]: listmail function called")
    list_arg = server_msg.split("#n#")
    # print(list_arg)
    print("\tID\tSubject\tFrom\tDate")
    for i in range(len(list_arg)):
        if i == len(list_arg)-1:
            break
        arg = list_arg[i].split("#$#")
        print("\t" + str(i+1), end = "")
        for element in arg:
            print("\t" + element, end = "")
        print("")

def retrmail(server_msg):
    list_arg = server_msg.split("#n#")
    tmp = list_arg[0].replace("\t", "")
    now_user = tmp
    print("\tSubject\t:" + list_arg[1])
    print("\tFrom\t:" + list_arg[2])
    tmp2 = "\tDate\t:2020/" + list_arg[3]
    print(tmp2.replace("/", "-"))
    print("\t--")
    filename = list_arg[4] + "mail.txt"
    filepath = "mailbox/" + filename
    # print(filename)
    # print(filepath)
    # print(list_arg)
    target_bucket = s3.Bucket("nphw0616037userx"+now_user.lower())
    target_object = target_bucket.Object(filepath)
    content = target_object.get()["Body"].read().decode() #get the content of hello.txt
    token = content.split("\n")
    # print(token)
    for i in range(len(token)):
        print("\t" + token[i])
    # print(content)

def deletemail(server_msg):
    list_arg = server_msg.split("#n#")
    now_user = list_arg[0]
    filename = list_arg[1] + "mail.txt"
    filepath = "mailbox/" + filename
    # print(filename)
    # print(filepath)
    target_bucket = s3.Bucket("nphw0616037userx"+now_user.lower())
    target_object = target_bucket.Object(filepath)
    target_object.delete()


arg = get_argments()
# print(arg.ip)
# print(arg.port)
# Host = "127.0.0.1"
Host = arg.ip
Port = arg.port
parserr = argparse.ArgumentParser(prog='PROG', description='description')
parserr.add_argument('cmd', choices=['register','delete','help','quit'])

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_s:
    server_s.connect((Host, Port))
    msg = server_s.recv(2048).decode("utf-8")
    print(str(msg), end="")
    while 1:
        input_cmd = input()
        if len(input_cmd) == 0:
            print("% ", end = "")
            continue
        sendmsg = input_cmd + "\r\n"
        # client_arg = (input_cmd.split())
        # print("("+input_cmd+")")
        # print(client_arg[0])
        server_s.send(sendmsg.encode("utf-8"))
        # if client_arg[0] != "exit":
        msg = server_s.recv(2048).decode("utf-8").strip(b'\x00'.decode())
        if not msg:
            break
        else:
            output_cmd = func(input_cmd, str(msg), now_user)
            print(output_cmd[0], end="")
    server_s.close()
