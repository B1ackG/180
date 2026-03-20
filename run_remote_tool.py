import pexpect
import sys
import time

password = "1234"
user = "root"
host = "192.168.1.245"
work_dir = "/userfs/app"
cmd_to_run = "./190_20260108"

print(f"Connecting to {user}@{host} to run {cmd_to_run}...")

ssh_cmd = f"ssh {user}@{host}"
child = pexpect.spawn(ssh_cmd)
child.logfile = sys.stdout.buffer

try:
    i = child.expect(['password:', 'yes/no'], timeout=30)
    if i == 0:
        child.sendline(password)
    elif i == 1:
        child.sendline("yes")
        child.expect('password:', timeout=30)
        child.sendline(password)
    
    # Wait for prompt (assuming # or $)
    # Use a generic prompt regex or just expect the shell to validly accept commands
    # If connection is slow, this might fail. Let's look for common prompts.
    child.expect(['#', '$', '>'], timeout=30)
    
    print(f"Navigating to {work_dir}...")
    child.sendline(f"cd {work_dir}")
    child.expect(['#', '$', '>'], timeout=10)
    
    print(f"Executing {cmd_to_run}...")
    child.sendline(f"export DISPLAY=:0") # Just in case it's a GUI app needing display env
    child.sendline(cmd_to_run)
    
    # Now we just let it run and stream output
    # pexpect.interact() is for real users, here we just want to keep the process alive
    # and let the logfile handle stdout.
    
    while True:
        try:
            # Expect newline to flush buffer or just wait
            # We assume the app prints things.
            child.expect('\n', timeout=None) 
        except pexpect.EOF:
            print("Remote process exited.")
            break
            
except Exception as e:
    print(f"Error: {e}")
    sys.exit(1)
