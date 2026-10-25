#include "scores.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
int main(void){
 remove("build-host/top10-test.txt");remove("build-host/top10-test.txt.bak");
 Scores s;scoresInit(&s);assert(scoresRank(&s,0,0)==-1);assert(scoresRank(&s,-1,42)==-1);
 for(int i=0;i<10;i++)assert(scoresInsert(&s,0,100+i*100,"A 1")==0);
 assert(s.count[0]==10&&s.entries[0][0].score==1000&&s.entries[0][9].score==100);
 assert(scoresRank(&s,0,100)==-1);assert(scoresRank(&s,0,99)==-1);assert(scoresRank(&s,0,1000)==1);
 assert(scoresInsert(&s,0,1100,"Z9 ")==0&&s.entries[0][9].score==200);
 assert(scoresInsert(&s,1,1234,"   ")==0);assert(scoresInsert(&s,2,INT_MAX,"ABC")==0);
 assert(scoresBest(&s,2)==INT_MAX&&scoresBest(&s,1)==1234);
 assert(scoresSave(&s,"build-host/top10-test.txt"));Scores loaded;scoresInit(&loaded);assert(scoresLoad(&loaded,"build-host/top10-test.txt"));assert(!memcmp(&s,&loaded,sizeof(s)));
 assert(scoresInsert(&s,1,1200,"9 Z")==1);assert(scoresSave(&s,"build-host/top10-test.txt"));assert(scoresLoad(&loaded,"build-host/top10-test.txt"));assert(!memcmp(&s,&loaded,sizeof(s)));
 Scores previous;assert(scoresLoad(&previous,"build-host/top10-test.txt.bak"));assert(previous.count[1]==1);
 /* Interrupted primary write: recover new complete temp, then old backup. */
 FILE*broken=fopen("build-host/top10-test.txt","w");assert(broken);fputs("TRUNCATED",broken);fclose(broken);
 assert(scoresSave(&s,"build-host/recovery-source.txt"));
 FILE*src=fopen("build-host/recovery-source.txt","r");FILE*dst=fopen("build-host/top10-test.txt.tmp","w");assert(src&&dst);int ch;while((ch=fgetc(src))!=EOF)fputc(ch,dst);fclose(src);fclose(dst);
 assert(scoresLoadRecover(&loaded,"build-host/top10-test.txt"));assert(!memcmp(&s,&loaded,sizeof(s)));
 remove("build-host/top10-test.txt.tmp");assert(scoresLoadRecover(&loaded,"build-host/top10-test.txt"));assert(!memcmp(&previous,&loaded,sizeof(previous)));
 assert(scoresSave(&s,"build-host/top10-test.txt"));
 assert(!scoresSave(&s,"build-host/missing-dir/top10.txt"));
 assert(strstr(scoresSaveError(),"WRITE TEMP ERR "));
 const char*blocked="build-host/replace-blocked";
 mkdir(blocked,0777);FILE*marker=fopen("build-host/replace-blocked/keep","w");assert(marker);fputs("keep",marker);fclose(marker);
 assert(!scoresSave(&s,blocked));assert(strstr(scoresSaveError(),"WRITE SCORES ERR "));
 assert(scoresLoad(&loaded,"build-host/replace-blocked.tmp"));assert(!memcmp(&s,&loaded,sizeof(s)));
 assert(access("build-host/replace-blocked/keep",F_OK)==0);
 remove("build-host/replace-blocked/keep");rmdir(blocked);remove("build-host/replace-blocked.tmp");
 assert(scoresSave(&s,"build-host/top10-test.txt"));assert(!scoresSaveError()[0]);
 const char*invalid[]={"WRONG\n","RETROARCADE_TOP10_V1\n11\n","RETROARCADE_TOP10_V1\n1\n100 61 41 41\n0\n0\n","RETROARCADE_TOP10_V1\n2\n100 41 41 41\n200 41 41 41\n0\n0\n","RETROARCADE_TOP10_V1\n1\n0 41 41 41\n0\n0\n","RETROARCADE_TOP10_V1\n\n0\n0\n","RETROARCADE_TOP10_V1\n0\n0\n0\nJUNK\n"};
 for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++){FILE*f=fopen("build-host/top10-bad.txt","w");assert(f);fputs(invalid[i],f);fclose(f);Scores before=loaded;assert(!scoresLoad(&loaded,"build-host/top10-bad.txt"));assert(!memcmp(&loaded,&before,sizeof(loaded)));}
 assert(scoreCharacter(' ')&&scoreCharacter('9')&&scoreCharacter('Z')&&!scoreCharacter('a')&&!scoreCharacter('-'));
 puts("PASS: independent top tens, ranking, ties, eviction, three-character names/spaces, max score, load/save/replace, failed saves, invalid/truncated files.");
}
