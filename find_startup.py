import pexpect
import sys

password = "1234"
user = "root"
host = "192.168.1.245"

def run_cmd(cmd):
    print(f"[Exec] {cmd}")
    child = pexpect.spawn(f"ssh {user}@{host}")
    child.logfile_read = sys.stdout.buffer
    try:
        i = child.expect(['password:', 'yes/no'], timeout=10)
        if i == 0:
            child.sendline(password)
        elif i == 1:
            child.sendline("yes")
            child.expect('password:', timeout=10)
            child.sendline(password)
        
        child.expect(['#', '$'], timeout=10)
        child.sendline(cmd)
        child.expect(['#', '$'], timeout=10)
        # We'll see output from logs
    except Exception as e:
        print(f"Error: {e}")
    finally:
        child.close()
        print(f"Error: {e}")
    finally:
        child.close()

if __name__ == "__main__":
    run_cmd("ls -F /etc/init.d/")
    run_cmd("ls -F /userfs/app/")
    run_cmd("ps aux | grep 190")
