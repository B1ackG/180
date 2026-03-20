import pexpect
import sys
import os

password = "1234"
src_file = "180"
user = "root"
host = "192.168.1.245"
dest_dir = "/userfs/app"

print(f"Starting SCP of {src_file} to {user}@{host}:{dest_dir}...")

cmd = f"scp {src_file} {user}@{host}:{dest_dir}"
child = pexpect.spawn(cmd)

try:
    i = child.expect(['password:', pexpect.EOF, 'yes/no'], timeout=30)
    if i == 0:
        child.sendline(password)
        child.expect(pexpect.EOF, timeout=60)
    elif i == 2:
        child.sendline("yes")
        child.expect('password:', timeout=30)
        child.sendline(password)
        child.expect(pexpect.EOF, timeout=60)
    
    print("SCP completed successfully.")
    print(child.before.decode(errors='ignore') if child.before else "")
    sys.exit(0)
    
except pexpect.TIMEOUT:
    print("SCP timed out.")
    print(child.before.decode(errors='ignore') if child.before else "")
    sys.exit(1)
except Exception as e:
    print(f"An error occurred: {str(e)}")
    sys.exit(1)
