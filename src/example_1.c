#include <stdio.h>
#include <co_routine_inner.h>
#include <co_routine.h>

#define P(msg) {printf("%s\n",msg);}


void *coroutineA(void *);
void *coroutineB(void *);

int main(){
	stCoRoutine_t *co = NULL;
	stCoRoutine_t *coB = NULL;
	co_create(&co, NULL, coroutineA, NULL);
	co_create(&coB, NULL, coroutineB, NULL);
	co_resume(co);
	P("a");
	P("b");
	co_resume(coB);
	co_resume(co);
	P("c");
	co_resume(coB);
	co_resume(co);
	P("d");
	co_resume(coB);

	co_release(co);
	co_release(coB);
	return 0;
}

void *coroutineA(void *){
	P("1");
	P("2");
	co_yield_ct();
	P("3");
	P("4");
	co_yield_ct();
	P("5");
}
void *coroutineB(void *){
	P("i");
	P("ii");
	co_yield_ct();
	P("iii");
	P("iiii");
	co_yield_ct();
	P("iiiii");
}
