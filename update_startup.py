import pexpect
import sys

password = "1234"
user = "root"
host = "192.168.1.245"
file_path = "/etc/init.d/app_setup.sh"

def run_commands():
    child = pexpect.spawn(f"ssh {user}@{host}", encoding='utf-8')
    child.logfile = sys.stdout
    try:
        # Step 0: SSH Login
        child.expect(['password:', 'yes/no'], timeout=10)
        child.sendline(password)
        child.expect('#', timeout=10)
        
        # Step 1: Backup
        print("\n[Step 1] Creating backup of app_setup.sh...")
        child.sendline(f"cp {file_path} {file_path}.bak_$(date +%Y%m%d_%H%M%S)")
        child.expect('#', timeout=10)
        
        # Step 2: Replace 190_20260108 with 180
        print("\n[Step 2] Replacing '190_20260108' with '180'...")
        child.sendline(f"sed -i 's/190_20260108/180/g' {file_path}")
        child.expect('#', timeout=10)
        
        # Step 3: Verify the change
        print("\n[Step 3] Verifying change...")
        child.sendline(f"grep '180' {file_path}")
        child.expect('#', timeout=10)
        
    except Exception as e:
        print(f"\nError: {e}")
    finally:
        child.close()

if __name__ == "__main__":
    run_commands()
