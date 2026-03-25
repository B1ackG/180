import pexpect
import sys

password = "1234"
user = "root"
host = "192.168.1.245"

def run_cmd(cmd):
    print(f"\n[Exec] {cmd}")
    child = pexpect.spawn(f"ssh {user}@{host}", encoding='utf-8')
    child.logfile = sys.stdout
    try:
        i = child.expect(['password:', 'yes/no'], timeout=10)
        if i == 0:
            child.sendline(password)
        elif i == 1:
            child.sendline("yes")
            child.expect('password:', timeout=10)
            child.sendline(password)
        
        # 显式等待提示符
        idx = child.expect(['#', '\$'], timeout=10)
        print(f"\nPrompt found (index {idx})")
        
        child.sendline(cmd)
        # 等待下一个提示符以确保命令完成
        child.expect(['#', '\$'], timeout=15)
        print("\nCommand finished.")
    except Exception as e:
        print(f"\nError in run_cmd: {e}")
    finally:
        child.close()

if __name__ == "__main__":
    run_cmd("grep -r '190_20260108' /etc /userfs /data 2>/dev/null")
    run_cmd("ls -F /etc/init.d/")
