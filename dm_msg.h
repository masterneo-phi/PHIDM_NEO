/* 会話メッセージの形式
通常と詠唱に空コメントが入っているのは
#で始まる名前のチートキャラによる荒らしを防ぐ為
(これが無いと #close等で周りに居るキャラが
切断される恐れがある)
*/
#define TLK_NORMAL              "/**/%name > %s\n"
#define TLK_PRIV                "[%name] > %s\n"
#define TLK_PSYCHO              "(%name) > %s\n"
#define TLK_AREA                "{%name} > %s\n"
#define TLK_PARTY               "\"%name\" > %s\n"
#define TLK_SPELL               "/**/%name > %s\n"
#define TLK_SILENCE             "......"

/* DM中の全メッセージ
これらを変更して任意の言語バージョンを作る事が可能 */

#ifdef DM_LANG_JP
#include "dm_msg_jp.h"
#else
#include "dm_msg_en.h"
#endif
