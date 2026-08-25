import pexpect
import sys
import time

password = "1234"
user = "root"
host = "192.168.1.245"
src_file = "180"
dest_dir = "/userfs/app"
cmd_to_run = "./180"

def scp_file():
    print(f"[Deploy] Copying {src_file} to {user}@{host}:{dest_dir}...")
    cmd = f"scp {src_file} {user}@{host}:{dest_dir}"
    child = pexpect.spawn(cmd)
    
    try:
        i = child.expect(['password:', 'yes/no', pexpect.EOF], timeout=30)
        if i == 0:
            child.sendline(password)
        elif i == 1:
            child.sendline("yes")
            child.expect('password:', timeout=30)
            child.sendline(password)
        
        child.expect(pexpect.EOF, timeout=120)  # Wait for transfer to finish
        print("[Deploy] SCP completed.")
    except Exception as e:
        print(f"[Deploy] SCP Failed: {e}")
        # We continue anyway, maybe it's already there or we just want to run

def run_remote():
    print(f"[Run] Connecting to {user}@{host}...")
    ssh_cmd = f"ssh {user}@{host}"
    child = pexpect.spawn(ssh_cmd)
    child.logfile_read = sys.stdout.buffer # Stream output to our stdout
    
    try:
        i = child.expect(['password:', 'yes/no'], timeout=30)
        if i == 0:
            child.sendline(password)
        elif i == 1:
            child.sendline("yes")
            child.expect('password:', timeout=30)
            child.sendline(password)
        
        # Expect prompt
        child.expect(['#', '$'], timeout=30)
        
        print(f"[Run] Navigating to {dest_dir}...")
        child.sendline(f"cd {dest_dir}")
        child.expect(['#', '$'], timeout=10)
        
        print(f"[Run] Executing {cmd_to_run}...")
        child.sendline(cmd_to_run)
        
        print("[Run] Application started. Streaming output (Ctrl+C to stop)...")
        
        # Keep alive and stream
        while True:
            try:
                child.expect('\n', timeout=None)
            except pexpect.EOF:
                print("\n[Run] Remote process ended.")
                break
                
    except KeyboardInterrupt:
        print("\n[Run] User stopped the script.")
        child.sendcontrol('c')
        child.close()
    except Exception as e:
        print(f"\n[Run] Error: {e}")

if __name__ == "__main__":
    scp_file()
    run_remote()
