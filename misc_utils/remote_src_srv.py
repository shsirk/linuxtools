#
# this script launches simple socket server to listen for source/path:line_number command requests from remote machine
# then it invoke code.exe on given path. (existing code.exe instance is used) 
# @shsirk
#
import sys
import socket
import subprocess
from thread import *

def servicethread(conn):
  cmds = []
  while True:
    data = conn.recv(1024)
    if not data: 
      break
    cmds.append(data)

  #came out of loop
  conn.close()
  cmd_line = "".join(cmds).strip()
  print '[*] source cmd: %s' % cmd_line
  subprocess.Popen(["C:\\Users\\krishs\\AppData\\Local\\Programs\\Microsoft VS Code\\code.exe", "-r", "-g", cmd_line],
                           close_fds=True, creationflags=0x00000008)

def src_cmd_server(host, port):
  s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  try:
    s.bind((host, port))
  except socket.error as msg:
    print '[-] bind failed. Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
    sys.exit()

  s.listen(10)
  print '[+] server is up now on port %d!' % port

  try:
    while 1:
      conn, addr = s.accept()
      print '[*] connected with ' + addr[0] + ':' + str(addr[1])
    
      start_new_thread(servicethread ,(conn,))
  except KeyboardInterrupt:
    print "[-] user interruption! killing server"

  s.close()
  print "[+] server shutdown done."


if __name__ == "__main__":
  port = 5555

  if len(sys.argv) == 2:
    port = int(sys.argv[1])

  src_cmd_server('', port)