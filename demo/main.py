from ab import AB
import time

def main():
    a, b = raw_input().split()
    time.sleep(2)
    print AB().sum(int(a), 2 * int(b))

if __name__ == '__main__':
    main()
