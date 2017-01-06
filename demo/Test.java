public class Test{
    public static int sum(int a, int b){
        return a + b;
    }

    public static void main(String[] args){
        System.out.println(sum(args[0], args[1]));
    }
}