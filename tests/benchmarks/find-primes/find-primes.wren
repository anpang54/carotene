
var limit = 10000

var isPrime = Fn.new{ |n|
    if(n == 0 || n == 1) {
        return false
    }
    var i = 2
    while(i < n) {
        if(n % i == 0) {
            return false
        }
        i = i + 1
    }
    return true
}

for(i in 0...limit) {
    if(isPrime.call(i)) {
        // System.print(i)
    }
}