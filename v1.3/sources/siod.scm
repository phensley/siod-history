'(SIOD: Scheme In One Defun
(c) Copyright 1988 George Carrette, g...@bu-it.bu.edu
For demonstration purposes only.
Optional Runtime Library for Release 1.3)

(define list (lambda n n))

(define (sublis l exp)
(if (cons? exp)
(cons (sublis l (car exp))
(sublis l (cdr exp)))
(let ((cell (assq exp l)))
(if cell (cdr cell) exp))))

(define (cadr x) (car (cdr x)))
(define (caddr x) (car (cdr (cdr x))))
(define (cdddr x) (cdr (cdr (cdr x))))

(define (replace before after)
(set-car! before (car after))
(set-cdr! before (cdr after))
after)

(define (push-macro form)
(replace form
(list 'set! (caddr form)
(list 'cons (cadr form) (caddr form)))))

(define (pop-macro form)
(replace form
(list 'let (list (list 'tmp (cadr form)))
(list 'set! (cadr form) '(cdr tmp))
'(car tmp))))

(define push 'push-macro)
(define pop 'pop-macro)

(define (defvar-macro form)
(list 'or
(list 'value-cell (list 'quote (cadr form)))
(list 'define (cadr form) (caddr form))))

(define defvar 'defvar-macro)

(define (defun-macro form)
(cons 'define
(cons (cons (cadr form) (caddr form))
(cdddr form))))

(define defun 'defun-macro)

(define setq set!)
(define progn begin)

(define the-empty-stream ())

(define empty-stream? null?)

(define (*cons-stream head tail-future)
(list head () () tail-future))

(define head car)

(define (tail x)
(if (car (cdr x))
(car (cdr (cdr x)))
(let ((value ((car (cdr (cdr (cdr x)))))))
(set-car! (cdr x) t)
(set-car! (cdr (cdr x)) value))))

(define (cons-stream-macro form)
(replace form
(list '*cons-stream
(cadr form)
(list 'lambda () (caddr form)))))

(define cons-stream 'cons-stream-macro)

(define (enumerate-interval low high)
(if (> low high)
the-empty-stream
(cons-stream low (enumerate-interval (+ low 1) high))))

(define (print-stream-elements x)
(if (empty-stream? x)
()
(begin (print (head x))
(print-stream-elements (tail x)))))

(define (standard-fib x)
(if (< x 2)
x
(+ (standard-fib (- x 1))
(standard-fib (- x 2)))))

