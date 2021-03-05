;; SIOD: Scheme In One Defun -*-mode:lisp;parser:read-*-
;;
;; *                        COPYRIGHT (c) 1989-1994 BY                       *
;; *        PARADIGM ASSOCIATES INCORPORATED, CAMBRIDGE, MASSACHUSETTS.      *
;; *        See the source file SLIB.C for more information.                 *
;;
;; $Id: siod.scm,v 1.3 1996/02/27 20:41:50 gjc Exp $
;;

(if (>= (verbose) 3)
    (puts  ";; Optional Runtime Library for Release 3.0\n"))

(set! *after-gc* '(if (< (gc-info 4) 5000) (allocate-heap)))

(define list (lambda n n))

(define (sublis l exp)
  (if (cons? exp)
      (cons (sublis l (car exp))
	    (sublis l (cdr exp)))
      (let ((cell (assq exp l)))
	(if cell (cdr cell) exp))))

(define (caar x) (car (car x)))
(define (cadr x) (car (cdr x)))
(define (cdar x) (cdr (car x)))
(define (cddr x) (cdr (cdr x)))

(define (caddr x) (car (cdr (cdr x))))
(define (cdddr x) (cdr (cdr (cdr x))))

(define consp pair?)

(define (replace before after)
  (set-car! before (car after))
  (set-cdr! before (cdr after))
  after)

(define (prognify forms)
  (if (null? (cdr forms))
      (car forms)
    (cons 'begin forms)))

(define (defmac-macro form)
  (let ((sname (car (cadr form)))
	(argl (cdr (cadr form)))
	(fname nil)
	(body (prognify (cddr form))))
    (set! fname (symbolconc sname '-macro))
    (list 'begin
	  (list 'define (cons fname argl)
		(list 'replace (car argl) body))
	  (list 'define sname (list 'quote fname)))))

(define defmac 'defmac-macro)

(defmac (push form)
  (list 'set! (caddr form)
	(list 'cons (cadr form) (caddr form))))

(defmac (pop form)
  (list 'let (list (list 'tmp (cadr form)))
	(list 'set! (cadr form) '(cdr tmp))
	'(car tmp)))

(defmac (defvar form)
  (list 'or
	(list 'symbol-bound? (list 'quote (cadr form)))
	(list 'define (cadr form) (caddr form))))

(defmac (defun form)
  (cons 'define
	(cons (cons (cadr form) (caddr form))
	      (cdddr form))))

(defmac (setq form)
  (let ((l (cdr form))
	(result nil))
    (define (loop)
      (if l
	  (begin (push (list 'set! (car l) (cadr l)) result)
		 (set! l (cddr l))
		 (loop))))
    (loop)
    (prognify (reverse result))))
  
  
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

(defmac (cons-stream form)
  (list '*cons-stream
	(cadr form)
	(list 'lambda () (caddr form))))

(define (enumerate-interval low high)
  (if (> low high)
      the-empty-stream
      (cons-stream low (enumerate-interval (+ low 1) high))))

(define (print-stream-elements x)
  (if (empty-stream? x)
      ()
      (begin (print (head x))
	     (print-stream-elements (tail x)))))

(define (sum-stream-elements x)
  (define (loop acc x)
    (if (empty-stream? x)
	acc
      (loop (+ (head x) acc) (tail x))))
  (loop 0 x))

(define (standard-fib x)
  (if (< x 2)
      x
      (+ (standard-fib (- x 1))
	 (standard-fib (- x 2)))))

(define (call-with-current-continuation fcn)
  (let ((tag (cons nil nil)))
    (*catch tag
	    (fcn (lambda (value)
		   (*throw tag value))))))

(define (loop-test n f)
  (let ((j 0)
	(k 0)
	(m 0)
	(result nil))
    (while (< j n)
      (setq j (+ 1 j))
      (setq k 0)
      (while (< k 99)
	(setq k (+ k 1))
	(setq m 0)
	(while (< m 99)
	  (setq m (+ m 1))
	  (if f (setq result (cons nil result))))))
    result))


(defun atom (x)
  (not (consp x)))

(define eq eq?)

(defmac (cond form)
  (cond-convert (cdr form)))

(define null null?)

(defun cond-convert (l)
  (if (null l)
      ()
    (if (null (cdar l))
	(if (null (cdr l))
	    (caar l)
	  (let ((rest (cond-convert (cdr l))))
	    (if (and (consp rest) (eq (car rest) 'or))
		(cons 'or (cons (caar l) (cdr rest)))
	      (list 'or (caar l) rest))))
      (if (or (eq (caar l) 't)
	      (and (consp (caar l)) (eq (car (caar l)) 'quote)))
	  (prognify (cdar l))
	(list 'if
	      (caar l)
	      (prognify (cdar l))
	      (cond-convert (cdr l)))))))

(defmac (+internal-comma form)
  (error 'comma-not-inside-backquote))

(define +internal-comma-atsign +internal-comma)
(define +internal-comma-dot +internal-comma)

(defmac (+internal-backquote form)
  (backquotify (cdr form)))

(defun backquotify (x)
  (let (a d aa ad dqp)
    (cond ((atom x) (list 'quote x))
	  ((eq (car x) '+internal-comma) (cdr x))
	  ((or (atom (car x))
	       (not (or (eq (caar x) '+internal-comma-atsign)
			(eq (caar x) '+internal-comma-dot))))
	   (setq a (backquotify (car x)) d (backquotify (cdr x))
		 ad (atom d) aa (atom a)
		 dqp (and (not ad) (eq (car d) 'quote)))
	   (cond ((and dqp (not (atom a)) (eq (car a) 'quote))
		  (list 'quote (cons (cadr a) (cadr d))))
		 ((and dqp (null (cadr d)))
		  (list 'list a))
		 ((and (not ad) (eq (car d) 'list))
		  (cons 'list (cons a (cdr d))))
		 (t (list 'cons a d))))
	  ((eq (caar x) '+internal-comma-atsign)
	   (list 'append (cdar x) (backquotify (cdr x))))
	  ((eq (caar x) '+internal-comma-dot)
	   (list 'nconc (cdar x)(backquotify (cdr x)))))))


(defun append n
  (appendl n))

(defun appendl (l)
  (cond ((null l) nil)
	((null (cdr l)) (car l))
	((null (cddr l))
	 (append2 (car l) (cadr l)))
	('else
	 (append2 (car l) (appendl (cdr l))))))

(defun append2 (a b)
  (if (null a)
      b
    (cons (car a) (append2 (cdr a) b))))

(defun rplacd (a b)
  (set-cdr! a b)
  a)

(defun nconc (a b)
  (if (null a)
      b
    (rplacd (last a) b)))


(defun last (a)
  (cond ((null a) (error'null-arg-to-last))
	((null (cdr a)) a)
	((last (cdr a)))))

(define sfib
  (eval `(lambda (x)
	   (,if (,< x 2)
	       x
	     (,+ (sfib (,- x 1))
		 (sfib (,- x 2)))))))
(define sloop-test
  (eval `(lambda (n f)
	   (let ((j 0)
		 (k 0)
		 (m 0)
		 (result nil))
	     (,while (,< j n)
	       (,set! j (,+ 1 j))
	       (,set! k 0)
	       (,while (,< k 99)
		 (,set! k (,+ k 1))
		 (,set! m 0)
		 (,while (,< m 99)
		   (,set! m (,+ m 1))
		   (,if f (,set! result (,cons () result))))))
	     result))))

(defvar *fasdump-hash* t)

(defun fasl-open (filename mode)
  (list (fopen filename mode)
	(if (or (equal? mode "rb") *fasdump-hash*)
	    (cons-array 100))
	;; If this is set NIL, then already hashed symbols will be
	;; optimized, and additional ones will not.
	0))

(defun fasl-close (table)
  (fclose (car table)))

(defun fasload args
  (let ((filename (car args))
	(head (and (cadr args) (cons nil nil))))
    (let ((table (fasl-open filename "rb"))
	  (exp)
	  (tail head))
      (while (not (eq table (setq exp (fast-read table))))
	(cond (head
	       (setq exp (cons exp nil))
	       (set-cdr! tail exp)
	       (setq tail exp))
	      ('else
	       (eval exp))))
      (fasl-close table)
      (and head (cdr head)))))

(defun fasdump (filename forms)
  (let ((table (fasl-open filename "wb"))
	(l forms))
    (while l
      (fast-print (car l) table)
      (setq l (cdr l)))
    (fasl-close table)))

(defun compile-file (filename)
  (let ((forms (load (string-append filename ".scm") t)))
    (puts "Saving forms
")
    (fasdump (string-append filename ".bin")
	     forms)))

(defvar *properties* (cons-array 100))

(defun get (sym key)
  (cdr (assq key (href *properties* sym))))

(defun putprop (sym val key)
  (let ((alist (href *properties* sym)))
    (let ((cell (assq key alist)))
      (cond (cell
	     (set-cdr! cell val))
	    ('else
	     (hset *properties* sym (cons (cons key val) alist))
	     val)))))

(define (mapcar1 f l1)
  (and l1 (cons (f (car l1)) (mapcar1 f (cdr l1)))))


(define (mapcar2 f l1 l2)
  (and l1 l2 (cons (f (car l1) (car l2)) (mapcar2 f (cdr l1) (cdr l2)))))

(define (mapcar . args)
  (cond ((null args)
	 (error "too few arguments"))
	((null (cdr args))
	 (error "too few arguments"))
	((null (cdr (cdr args)))
	 (mapcar1 (car args) (car (cdr args))))
	((null (cdr (cdr (cdr args))))
	 (mapcar2 (car args) (car (cdr args)) (car (cdr (cdr args)))))
	('else
	 (error "two many arguments"))))
	
	 
(defun addl (l)
  (let ((sum 0))
    (while l
      (setq sum (+ sum (pop l))))
    sum))
