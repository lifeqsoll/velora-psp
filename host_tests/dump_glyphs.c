#include "shell/font.h"
#include <stdio.h>
static void p(int x,int y,ShellRgb c,void*ud){(void)c;(void)ud;printf("%d,%d\n",x,y);}
int main(void){
  const char *s="ms0:/ISO";
  int i; for(i=0;s[i];i++){
    const unsigned char *g=shell_font_glyph((unsigned)(unsigned char)s[i]);
    printf("'%c':", s[i]);
    for(int r=0;r<8;r++) printf(" %02X", g[r]);
    printf("\n");
  }
  /* dump em-like visual */
  for(i=0;s[i];i++){
    const unsigned char *g=shell_font_glyph((unsigned)(unsigned char)s[i]);
    printf("--- %c ---\n", s[i]);
    for(int r=0;r<8;r++){
      for(int x=0;x<8;x++) putchar(g[r]&(0x80>>x)?'#':'.');
      putchar('\n');
    }
  }
  return 0;
}
