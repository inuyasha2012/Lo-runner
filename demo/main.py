from ab import AB

c = 1

def main():
    a, b = raw_input().split()
    print AB().sum(int(a), int(b))

if __name__ == '__main__':
    main()
