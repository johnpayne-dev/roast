# roast - The Decaf Compiler

Decaf is a derivative of the C language that is simplified to just basic types (int, bool) function calls, and other control flow structures. Roast compiles decaf programs to x86 assembly and supports MacOS and Linux. Basic optimizations are implemented like constant folding, copy propagation, dead code elimination, etc.

## Examples

Input:
```
import printf;

void main()
{
	printf("Hello Decaf\n");
}
```
Output:
```
$ roast main.dcf
.data
string_1:
	.string "Hello Decaf\n"
	.align 16
.text
.globl _main
_main:
	pushq %rbp
	movq %rsp, %rbp
block_0:
	leaq string_1(%rip), %rdi
	movq $0, %rax
	call _printf
	movq %rax, -8(%rbp)
	movq $0, %rax
	movq %rbp, %rsp
	popq %rbp
	ret
```

Input:
```
import printf;

int factorial(int n)
{
	if (n <= 1) {
		return 1;
	} else {
		return n * factorial(n - 1);
	}
}

void main() {
	int res;
	res = factorial(10);
	printf("10! = %d (3628800)\n", res);
}
```

Output:
```
.data
string_1:
	.string "10! = %d (3628800)\n"
	.align 16
.text
.globl _factorial
_factorial:
	pushq %rbp
	movq %rsp, %rbp
	subq $96, %rsp
	movq %rdi, -8(%rbp)
block_0:
	movq -8(%rbp), %r10
	movq $1, %r11
	cmpq %r11, %r10
	setle %al
	andb $1, %al
	movzbq %al, %r10
	movq %r10, -32(%rbp)
	movq -32(%rbp), %r11
	movq $0, %r10
	cmpq %r10, %r11
	je block_2
block_1:
	movq $1, %rax
	movq %rbp, %rsp
	popq %rbp
	ret
block_3:
	jmp block_4
block_2:
	movq -8(%rbp), %r10
	movq $1, %r11
	subq %r11, %r10
	movq %r10, -80(%rbp)
	movq -80(%rbp), %rdi
	movq $0, %rax
	call _factorial
	movq %rax, -56(%rbp)
	movq -8(%rbp), %r10
	movq -56(%rbp), %r11
	imulq %r11, %r10
	movq %r10, -88(%rbp)
	movq -88(%rbp), %rax
	movq %rbp, %rsp
	popq %rbp
	ret
block_5:
block_4:
	movq $-2, %rdi
	call _exit
.globl _main
_main:
	pushq %rbp
	movq %rsp, %rbp
	subq $48, %rsp
block_6:
	movq $10, %rdi
	movq $0, %rax
	call _factorial
	movq %rax, -16(%rbp)
	leaq string_1(%rip), %rdi
	movq -16(%rbp), %rsi
	movq $0, %rax
	call _printf
	movq %rax, -32(%rbp)
	movq $0, %rax
	movq %rbp, %rsp
	popq %rbp
	ret
```

