%{
#include <string.h>
#define YYSTYPE char *
#include "y.tab.h"
%}

%%

\n  {
        /* return NEWLINE; */
    }

\t {
        /* discard tab */
    }
" " {
        /* discard space */
    }

">" {
        return GREAT;
    }

"<" {
        return LESS;
    }

">>" {
        return GREATGREAT;
    }

"<<" {
        return LESSLESS;
    }

">&" {
        return GREATAMPERSAND;
    }

">>&" {
        return GREATGREATAMPERSAND; 
    }

"<&" {
        return LESSAMPERSAND;
    }

"|" {
        return PIPE;
    }

"&" {
        return AMPERSAND;
    }

";" {
        return SEMICOLON;
    }

[^";"" "">"">>""<"">&"">>&""|""&"\t\n]+ {
        yylval=strdup(yytext);
        //printf("Got word token: '%s'\n", yytext);
        return WORD;
}

. {
        /* Other cases. */
}

%%

