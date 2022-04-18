#include <applib/buffer.h>
#include <capidl.h>

typedef struct MyLexer MyLexer;
struct MyLexer {
  InternedString current_file;
  InternedString current_basename;
  int current_line;
  int num_errors;
  int curlyDepth;
  bool isActiveUOC;

  Buffer *current_doc_comment;
};

MyLexer *mylexer_create(FILE *fin, bool isActiveUOC);
void mylexer_setDebug(bool);

void mylexer_ReportParseError(MyLexer *, const char * /* msg */);
InternedString mylexer_grab_doc_comment(MyLexer *);

typedef struct PrescanLexer PrescanLexer;
struct PrescanLexer {
  InternedString pkgName;
  InternedString fileName;

  bool isCmdLine;
  int current_line;
  int commentCaller;
  int curlyDepth;
};

int prescan_lex(ParseType *lvalp, PrescanLexer *lexer);

PrescanLexer *
prescanlexer_create(const char *inputFileName, 
		    bool isCmdLine, FILE *fin);
void prescanlexer_setDebug(bool showParse);

#if 0

/* Now THIS is REALLY STUPID. Why didn't it occur to the Flex people
   that you might want to call a reentrant lexer from a reentrant
   parser? Duh. */
struct MyLexer : public yyFlexLexer {
  ParseType *yylval;

  InternedString current_file;
  InternedString current_basename;
  int current_line;
  int num_errors;
  int curlyDepth;

  bool have_doc_comment;
  InternedString current_doc_comment;

  int yylex(ParseType *lvalp);

  void ReportParseError(const char * /* msg */);
  
  InternedString mylexer_grab_doc_comment(MyLexer *);

  MyLexer( istream* arg_yyin = 0, ostream* arg_yyout = 0)
    : yyFlexLexer(arg_yyin, arg_yyout)
  {
    current_line = 1;
    num_errors = 0;
    curlyDepth = 0;

    have_doc_comment = false;
  }
};

struct PrescanLexer : public yyFlexLexer {
  InternedString pkgName;
  InternedString fileName;

  int current_line;
  int commentCaller;
  int curlyDepth;

  int yylex();

  PrescanLexer( const char *inputFileName,
		istream* arg_yyin = 0, ostream* arg_yyout = 0)
    : yyFlexLexer(arg_yyin, arg_yyout), 
		 fileName(inputFileName)
  {
    current_line = 1;
  }
};



#endif
