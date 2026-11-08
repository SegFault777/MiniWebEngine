#include "../browser/tls/tls.h"
#include <stdio.h>
int main(void){int bad=0,ec=0,rsa=0;
 for(int r=0;r<TRUSTED_ROOT_TOTAL;r++){x509_cert_t c;
  if(!x509_parse(trusted_roots[r],trusted_root_lens[r],&c)){printf("PARSE FAIL %d\n",r);bad++;continue;}
  if(c.ec_group)ec++;else if(c.pubkey_valid)rsa++;else{printf("NO KEY %d\n",r);bad++;}
  if(!c.is_ca){printf("not CA flag %d\n",r);} }
 printf("roots=%d rsa=%d ecdsa=%d bad=%d\n",TRUSTED_ROOT_TOTAL,rsa,ec,bad);return bad;}
