from ab import AB


def main():
    a, b = raw_input().split()
    print AB().sum(int(a), 2 * int(b))

if __name__ == '__main__':
    main()
