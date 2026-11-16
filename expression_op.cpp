#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string_view>

#include "phi_dm.h"
#include "phi_calc.h"
#include "phi_data.h"
#include "expression_op.h"

/* 式のコンパイルと評価(スイッチデータの動作 50「式計算」)。
 *
 * 2つの段階がある。コンパイル(get_sentence 系)は phi.swt の読み込み
 * 時に式文字列を ATOM の列(内部コード)に変える。評価
 * (calc_expression)はスイッチの発動時に内部コードをスタックマシン
 * で実行する。コンパイル結果はスイッチごとに ATOM 配列(swtd)へ
 * 写され、評価はその配列を先頭から読む。
 *
 * 内部コードの1トークンは、種類(kind)に続けて値(val)を置き、
 * 変数以上の種類ではさらに型(type)、配列なら大きさ(size)を置く
 * (put_token / get_codetoken)。EXP_TERMINATE と EXP_RESET は
 * 種類だけで値を置かない。
 *
 * 評価スタックはこのファイルの stack と、expression_op.h で
 * extern 宣言された stpt である。評価の前に phi_data.cpp が
 * stpt = 0 に戻す。コンパイルは再帰下降で、低い優先順位の
 * 演算子は get_sentence の再帰で右辺を読み、高い優先順位の
 * 演算子は get_token で1トークンだけ読む(get_sentence 参照)。
 */

namespace {

// log_print() は引数をログファイルへ書いて、log_send がいれば
// check_write() で送るだけで、引数のバッファへは書き込まない
// (check_write は別バッファへ写してから送る)。文字列リテラル
// (ERRM_* マクロ)を渡すための窓口。
inline void log_error(const char *mes)
{
	log_print(const_cast<char *>(mes));
}

// 評価スタック。他の翻訳単位からは使わない。
std::array<ACUM, MAX_STACKS> stack;

// 参照変数 $name の表。順序は元の if-else の鎖と同じ。
// 接頭辞の重なりはない(pay と player、tax と talk は3文字目で
// 区別される)が、念のため順序は変えないこと。
struct SysVarEntry {
	std::string_view keyword;
	char name;
};

constexpr SysVarEntry kSysVars[] = {
	{"pay", 'p'},
	{"talk", 'p'},
	{"tax", 't'},
	{"time", 'T'},
	{"totalwp", 'W'},
	{"ATP", 'A'},
	{"DEF", 'D'},
	{"match", 'm'},
	{"wpn", 'w'},	/* add for check equip 20020530 */
	{"arm", 'a'},
	{"acc", 'o'},
	/* ↓スイッチデータ追加 2005/11/26 Savarl.F */
	{"player", 'P'},	/* PC(1)かNPC(0)か */
	/* ↑ADD */
};

} // namespace

int stpt; /* single definition; declared extern in expression_op.h */
extern ItemData putbuf;

/* 文字列からトークンを取得 */
/* 戻り値: 0 = OK / -1 = NG */
int get_token(char **p,int *lpt,ATOM *abf)
{
	ACUM a;
	int  i;

	SKIP_SPACE(*p);
	if (**p == '(') {
		(*p)++;
		return (get_sentence(p,lpt,abf,')'));
	} else if (**p == '[') {
		(*p)++;
		return (get_sentence(p,lpt,abf,']'));
	} else if (**p == '{') {
		(*p)++;
		return (get_sentence(p,lpt,abf,'}'));
	} else if (!std::strncmp(*p,"rnd(",4)) {	/* 関数 */
		(*p)+=4;
		if (get_sentence(p,lpt,abf,')')<0)
			return (-1);
		a.kind = EXP_FUNCTION;
		a.FUNCTION = FUNC_RND;
		if (put_token(a,lpt,abf) < 0)
			return (-1);
		return(0);
	} else if (**p >= 'x' && **p <= 'z') {	/* 汎用変数 */
		a.kind = EXP_VARIABLE;
		a.type = VAR_INT_A;
		a.NAME = **p;
		a.size = (**p == 'x' ? GVAR_MAX : (**p == 'y' ? PVAR_MAX : SVAR_MAX));
		(*p)++;
		if (put_token(a,lpt,abf) < 0)
			return (-1);
		return (get_arrayaffix(p,lpt,abf));
	} else if (**p == '$') {	/* 参照変数 */
		(*p)++;
		a.kind = EXP_SYSVAR;
		const SysVarEntry *found = nullptr;
		for (const SysVarEntry &e : kSysVars) {
			if (std::strncmp(*p,e.keyword.data(),e.keyword.size()) == 0) {
				found = &e;
				break;
			}
		}
		if (found == nullptr) {
			log_error(ERRM_UNDEF_SYSVAR);
			return (-1);
		}
		(*p) += found->keyword.size();
		a.NAME = found->name;
		return (put_token(a,lpt,abf));
	} else if (**p == '@') {	/* オブジェクト */
		(*p)++;
		a.kind = EXP_VARIABLE;
		if (!std::strncmp(*p,"CHR",3)) {	/* キャラクター */
			(*p) += 3;
			a.type = VAR_OCHR;
			a.NAME = 'C';
			if (put_token(a,lpt,abf) < 0)
				return (-1);
			return (get_CHRelement(p,lpt,abf));
		} else if (!std::strncmp(*p,"Inv",3)) {	/* 所持品 */
			(*p) += 3;
			a.type = VAR_ITEM_A;
			a.NAME = 'I';
			a.size = MAX_ITEMS;
			if (put_token(a,lpt,abf) < 0)
				return (-1);
			if (get_arrayaffix(p,lpt,abf) < 0)
				return (-1);
			return (get_itemelement(p,lpt,abf));
		} else if (!std::strncmp(*p,"item",4)) {	/* 条件合致品 */
			(*p) += 4;
			a.type = VAR_ITEM;
			a.NAME = 'i';
			if (put_token(a,lpt,abf) < 0)
				return (-1);
			return (get_itemelement(p,lpt,abf));
		} else if (!std::strncmp(*p,"wpn",3)) {	/* 武器 */
			(*p) += 3;
			a.type = VAR_ITEM;
			a.NAME = 'w';
			if (put_token(a,lpt,abf) < 0)
				return (-1);
			return (get_itemelement(p,lpt,abf));
		} else if (!std::strncmp(*p,"arm",3)) {	/* 鎧 */
			(*p) += 3;
			a.type = VAR_ITEM;
			a.NAME = 'a';
			if (put_token(a,lpt,abf) < 0)
				return (-1);
			return (get_itemelement(p,lpt,abf));
		} else if (!std::strncmp(*p,"acc",3)) {	/* アクセ */
			(*p) += 3;
			a.type = VAR_ITEM;
			a.NAME = 'o';
			if (put_token(a,lpt,abf) < 0)
				return (-1);
			return (get_itemelement(p,lpt,abf));
		} else if (!std::strncmp(*p,"Book",4)) {	/* 所持魔法 */
			(*p) += 4;
			a.type = VAR_MAGIC_A;
			a.NAME = 'B';
			a.size = MAX_MAGICS;
			if (put_token(a,lpt,abf) < 0)
				return (-1);
			if (get_arrayaffix(p,lpt,abf) < 0)
				return (-1);
			/* 所持魔法でも要素は get_itemelement で読む
			 * (get_magicelement ではない)。従来の挙動。 */
			return (get_itemelement(p,lpt,abf));
		} else if (!std::strncmp(*p,"magic",5)) {	/* 条件合致魔法 */
			(*p) += 5;
			a.type = VAR_MAGIC;
			a.NAME = 'm';
			if (put_token(a,lpt,abf) < 0)
				return (-1);
			return (get_magicelement(p,lpt,abf));
		} else {
			log_error(ERRM_UNDEF_OBJ);
			return (-1);
		}
	} else {
		a.kind = EXP_CONST;
		i = decimal_load_arbitrarily(p);
		if (i < 0) {
			log_error(ERRM_INVALID_STRING);
			return (-1);
		}
		a.VAL = i;
		return (put_token(a,lpt,abf));
	}
}

/* 配列添字取得。`[式]` の形か、16進数そのままの形のどちらかで
 * 添字を読み、最後に `[` 演算子を置く。添字は1から数える:
 * `[` の評価時に1を引くため、16進数の形では読んだ値に1を足して
 * 覚える。`x0` と `x[1]` は同じ要素になる。16進数は小文字の
 * a-f だけを受け付ける(hexadecimal_load_arbitrarily の仕様)。 */
int get_arrayaffix(char **p,int *lpt,ATOM *abf)
{
	ACUM a;
	int  i;

	SKIP_SPACE(*p);
	if (**p == '[') {
		(*p)++;
		if (get_sentence(p,lpt,abf,']') < 0)
			return (-1);
	} else {
		a.kind = EXP_CONST;
		i = hexadecimal_load_arbitrarily(p);
		if (i < 0) {
			log_error(ERRM_INVALID_STRING);
			return (-1);
		}
		a.VAL = i + 1;
		if (put_token(a,lpt,abf))
			return (-1);
	}
	a.kind = EXP_OPERATOR;
	a.OPERATOR = '[';
	return (put_token(a,lpt,abf));
}

/* CHR Obj 要素取得。`.要素名` の形で、CHRData の先頭からの
 * バイト差分(OFFSET)を要素トークンに覚える。評価時に
 * EXP_ELEMENT がベースアドレスへ OFFSET を足す。
 * 1文字の x / y の検査が cx / bx より先にあるが、cx と bx は
 * c / b で始まるため重ならない。Item / Magic だけは配列で、
 * 添字と要素の取得へ再帰する。 */
int get_CHRelement(char **p,int *lpt,ATOM *abf)
{
	ACUM a;

	if (**p == '.') {
		(*p)++;
		a.kind = EXP_ELEMENT;
		if (**p == 'x') {
			(*p)++;
			a.type = VAR_INT;
			a.OFFSET = ((char*)&(cd[0].x) - (char*)&(cd[0]));
		} else if (**p == 'y') {
			(*p)++;
			a.type = VAR_INT;
			a.OFFSET = ((char*)&(cd[0].y) - (char*)&(cd[0]));
		} else if (!std::strncmp(*p,"cx",2)) {
			(*p) += 2;
			a.type = VAR_INT;
			a.OFFSET = ((char*)&(cd[0].ix) - (char*)&(cd[0]));
		} else if (!std::strncmp(*p,"cy",2)) {
			(*p) += 2;
			a.type = VAR_INT;
			a.OFFSET = ((char*)&(cd[0].iy) - (char*)&(cd[0]));
		} else if (!std::strncmp(*p,"bx",2)) {
			(*p) += 2;
			a.type = VAR_INT;
			a.OFFSET = ((char*)&(cd[0].bx) - (char*)&(cd[0]));
		} else if (!std::strncmp(*p,"by",2)) {
			(*p) += 2;
			a.type = VAR_INT;
			a.OFFSET = ((char*)&(cd[0].by) - (char*)&(cd[0]));
		} else if (!std::strncmp(*p,"dir",3)) {
			(*p) += 3;
			a.type = VAR_INT;
			a.OFFSET = ((char*)&(cd[0].d) - (char*)&(cd[0]));
		} else if (!std::strncmp(*p,"flv",3)) {
			(*p) += 3;
			a.type = VAR_INT;
			a.OFFSET = ((char*)&(cd[0].lv[LV_FIGHTER]) - (char*)&(cd[0]));
		} else if (!std::strncmp(*p,"wlv",3)) {
			(*p) += 3;
			a.type = VAR_INT;
			a.OFFSET = ((char*)&(cd[0].lv[LV_WIZARD]) - (char*)&(cd[0]));
		} else if (!std::strncmp(*p,"mlv",3)) {
			(*p) += 3;
			a.type = VAR_INT;
			a.OFFSET = ((char*)&(cd[0].lv[LV_MERCHANT]) - (char*)&(cd[0]));
		} else if (!std::strncmp(*p,"clv",3)) {
			(*p) += 3;
			a.type = VAR_INT;
			a.OFFSET = ((char*)&(cd[0].lv[LV_CREATER]) - (char*)&(cd[0]));
		} else if (!std::strncmp(*p,"hp",2)) {
			(*p) += 2;
			a.type = VAR_LONG;
			a.OFFSET = ((char*)&(cd[0].hp) - (char*)&(cd[0]));
		} else if (!std::strncmp(*p,"mp",2)) {
			(*p) += 2;
			a.type = VAR_LONG;
			a.OFFSET = ((char*)&(cd[0].mp) - (char*)&(cd[0]));
		} else if (!std::strncmp(*p,"mhp",3)) {
			(*p) += 3;
			a.type = VAR_LONG;
			a.OFFSET = ((char*)&(cd[0].mhp) - (char*)&(cd[0]));
		} else if (!std::strncmp(*p,"mmp",3)) {
			(*p) += 3;
			a.type = VAR_LONG;
			a.OFFSET = ((char*)&(cd[0].mmp) - (char*)&(cd[0]));
		} else if (!std::strncmp(*p,"gp",2)) {
			(*p) += 2;
			a.type = VAR_INT;
			a.OFFSET = ((char*)&(cd[0].gp) - (char*)&(cd[0]));
		} else if (!std::strncmp(*p,"exp",3)) {
			(*p) += 3;
			a.type = VAR_INT;
			a.OFFSET = ((char*)&(cd[0].exp) - (char*)&(cd[0]));
		} else if (!std::strncmp(*p,"karma",5)) {
			(*p) += 5;
			a.type = VAR_INT;
			a.OFFSET = ((char*)&(cd[0].align) - (char*)&(cd[0]));
		} else if (!std::strncmp(*p,"Item",4)) {
			(*p) += 4;
			a.type = VAR_ITEM_A;
			a.OFFSET = ((char*)&(cd[0].id[0]) - (char*)&(cd[0]));
			a.size = MAX_ITEMS;
			if (put_token(a,lpt,abf) < 0)
				return (-1);
			if (get_arrayaffix(p,lpt,abf) < 0)
				return (-1);
			return (get_itemelement(p,lpt,abf));
		} else if (!std::strncmp(*p,"Magic",5)) {
			(*p) += 5;
			a.type = VAR_MAGIC_A;
			a.OFFSET = ((char*)&(cd[0].md[0]) - (char*)&(cd[0]));
			a.size = MAX_MAGICS;
			if (put_token(a,lpt,abf) < 0)
				return (-1);
			if (get_arrayaffix(p,lpt,abf) < 0)
				return (-1);
			return (get_magicelement(p,lpt,abf));
		} else if (!std::strncmp(*p,"msg",3)) {
			(*p) += 3;
			a.type = VAR_INT;
			a.OFFSET = ((char*)&(cd[0].anmf) - (char*)&(cd[0]));
		} else if (!std::strncmp(*p,"mwait",5)) {
			(*p) += 5;
			a.type = VAR_INT;
			a.OFFSET = ((char*)&(cd[0].anmt) - (char*)&(cd[0]));
		} else {
			log_error(ERRM_INVALID_ELEMENT);
			return (-1);
		}
		return (put_token(a,lpt,abf));
	}
	return (0);
}

/* Item Obj 要素取得。OFFSET は ItemData の先頭からのバイト差分。
 * prm だけは配列(op[1] から3要素)で、添字の取得へ再帰する。 */
int get_itemelement(char **p,int *lpt,ATOM *abf)
{
	ACUM a;

	if (**p == '.') {
		(*p)++;
		a.kind = EXP_ELEMENT;
		if (!std::strncmp(*p,"type",4)) {
			(*p) += 4;
			a.type = VAR_INT;
			a.OFFSET = ((char*)&(cd[0].id[0].type) - (char*)&(cd[0].id[0]));
		} else if (!std::strncmp(*p,"wp",2)) {
			(*p) += 2;
			a.type = VAR_SHORT;
			a.OFFSET = ((char*)&(cd[0].id[0].wp) - (char*)&(cd[0].id[0]));
		} else if (!std::strncmp(*p,"fn",2)) {
			(*p) += 2;
			a.type = VAR_CHAR;
			a.OFFSET = ((char*)&(cd[0].id[0].fn) - (char*)&(cd[0].id[0]));
		} else if (!std::strncmp(*p,"ATP",3)) {
			(*p) += 3;
			a.type = VAR_SHORT;
			a.OFFSET = ((char*)&(cd[0].id[0].ATP) - (char*)&(cd[0].id[0]));
		} else if (!std::strncmp(*p,"prm",3)) {
			(*p) += 3;
			a.type = VAR_SHORT_A;
			a.OFFSET = ((char*)&(cd[0].id[0].op[1]) - (char*)&(cd[0].id[0]));
			a.size = 3;
			if (put_token(a,lpt,abf) < 0)
				return (-1);
			return (get_arrayaffix(p,lpt,abf));
		} else {
			log_error(ERRM_INVALID_ELEMENT);
			return (-1);
		}
		return (put_token(a,lpt,abf));
	}
	return (0);
}

/* Magic Obj 要素取得。OFFSET は MagicData の先頭からのバイト差分。
 * 型は常に VAR_CHAR。 */
int get_magicelement(char **p,int *lpt,ATOM *abf)
{
	ACUM a;

	if (**p == '.') {
		(*p)++;
		a.kind = EXP_ELEMENT;
		a.type = VAR_CHAR;
		if (!std::strncmp(*p,"fn",2)) {
			(*p) += 2;
			a.OFFSET = ((char*)&(cd[0].md[0].fn) - (char*)&(cd[0].md[0]));
		} else if (!std::strncmp(*p,"effect",6)) {
			(*p) += 6;
			a.OFFSET = ((char*)&(cd[0].md[0].effect) - (char*)&(cd[0].md[0]));
		} else {
			log_error(ERRM_INVALID_ELEMENT);
			return (-1);
		}
		return (put_token(a,lpt,abf));
	}
	return (0);
}

/* トークン格納。1トークンは kind、val、(変数以上なら)type、
 * (配列なら)size の順に ATOM へ並べる。EXP_TERMINATE と
 * EXP_RESET は kind だけを置く。書く ATOM ごとに容量を確かめ、
 * 足りなければその場で止めて -1 を返す(途中まで書いた分は
 * 残る)。 */
int put_token(ACUM acum,int *lpt,ATOM *abf)
{
	if (*lpt >= ATOMBUF_SIZE) {
		log_error(ERRM_LONG_EXPRESSION);
		return (-1);
	}

	abf[(*lpt)++].di = acum.kind;
	if (acum.kind == EXP_TERMINATE ||
		acum.kind == EXP_RESET)
		return (0);

	if (*lpt >= ATOMBUF_SIZE) {
		log_error(ERRM_LONG_EXPRESSION);
		return (-1);
	}
	abf[(*lpt)++]    = acum.val;

	if (acum.kind >= EXP_VARIABLE) {
		if (*lpt >= ATOMBUF_SIZE) {
			log_error(ERRM_LONG_EXPRESSION);
			return (-1);
		}
		abf[(*lpt)++].di = acum.type;

		if (acum.type & VAR_ARRAY) {
			if (*lpt >= ATOMBUF_SIZE) {
				log_error(ERRM_LONG_EXPRESSION);
				return (-1);
			}
			abf[(*lpt)++].di = acum.size;
		}
	}
	return (0);
}

/* 文字列から内部コード列を生成。term までの式を読み、逆ポーランド
 * 記法の ATOM 列へ変える。
 *
 * 優先順位は2段しかない。`, + - = < > ! <= >= ==` は低い方で、
 * 右辺を get_sentence の再帰で読む(右の式を先に吐いてから
 * 演算子を置く)。`* / % & | ^` は高い方で、get_token で
 * 1トークンだけ読む。`1+2*3` は 1 2 3 * + になり 7 になる。
 *
 * f は「値を読んだ直後か」の印で、default の場合にだけ立つ。
 * 演算子の後では立たないため、`(x)+0` の `+` のような、括弧の
 * 直後の `+` / `-` は単項として読み飛ばされる(従来の挙動)。
 *
 * recall は再帰の制御用で、低い演算子で再帰するときに1にする。
 * 内側の呼び出しは termnext にそれを受け取り、終端文字を
 * 消費せず、EXP_TERMINATE も出さずに帰る。終端文字の処理は
 * 外側のループに任せる。
 *
 * `,` は文の区切りで、EXP_RESET を出して読み続ける。評価時に
 * EXP_RESET はスタックを空にする。行末では EXP_TERMINATE を
 * 出す。term が括弧のときは閉じ括弧を消費して帰る。 */
int get_sentence(char **p,int *lpt,ATOM *abf,char term)
{
	ACUM a1;
	char op,nop;
	int i;
	std::array<char, 256> buf;
	int f;

	static int recall = 0;

	int termnext;

	termnext = recall;
	recall = 0;

	f = 0;
	while (1) {
		SKIP_SPACE(*p);
		if (**p == term) {
			if (!termnext)
				(*p)++;
			if (term != '\0')
				return (0);
		}
		switch (**p) {
			case '\0':
			case '\n':
			case ',':
				if (term != '\0') {
					std::snprintf(buf.data(),buf.size(),ERRM_UNMATCH_BRACKET,term);
					log_print(buf.data());
				}
				if (!termnext) {
					a1.kind = (**p == ',' ? EXP_RESET : EXP_TERMINATE);
					if (put_token(a1,lpt,abf) < 0)
						return (-1);
					if (**p == ',') {
						(*p)++;
						break;
					}
				}
				return (term != '\0' ? -1 : 0);
			case '-':
				if (!f) {
					(*p)++;
					if (get_token(p,lpt,abf)<0)
						return(-1);
					a1.kind = EXP_OPERATOR;
					a1.OPERATOR = OP_REVERSE;
					if (put_token(a1,lpt,abf) < 0)
						return (-1);
					break;
				}
				/* 値の直後の - は二項演算子。+ の場合へ落ちる。 */
			case '+':
				if (!f) {
					(*p)++;
					break;
				}
				/* 値の直後の + は二項演算子。下の群へ落ちる。 */
			case '*':
			case '/':
			case '%':
			case '&':
			case '|':
			case '^':
				op = *(*p)++;
				if ((nop = **p) == '=') {
					(*p)++;
					a1.kind = EXP_OPERATOR;
					a1.OPERATOR = OP_STACKTOPCOPY;
					if (put_token(a1,lpt,abf) < 0)
						return (-1);
				}
			/* change for status check 20041218 */
				goto GET_TOKEN_NEXT;
			case '<':
			case '>':
			case '=':
			case '!':
				nop = '\0'; /* fix for un-init bug 20050510 */
				op = *(*p)++;
				if (**p == '=') {
					(*p)++;
					switch (op) {
						case '<':
							op = OP_LESSEQUAL;
							break;
						case '>':
							op = OP_GREATEREQUAL;
							break;
						case '=':
							op = OP_EQUAL;
							break;
						case '!':
							break;
						default:
							return (-1);
					}
				}
				GET_TOKEN_NEXT:
				a1.kind = EXP_OPERATOR;
				a1.OPERATOR = op;
				switch (op) {
					case ',':
					case '+':
					case '-':
					case '=':
					case '<':
					case '>':
					case '!':
					case OP_LESSEQUAL:
					case OP_GREATEREQUAL:
					case OP_EQUAL:
						recall = 1;
						i = get_sentence(p,lpt,abf,term);
						break;
					case '*':
					case '/':
					case '%':
					case '&':
					case '|':
					case '^':
					default:
						i = get_token(p,lpt,abf);
						break;
				}
				if (i < 0)
					return (-1);
				if (put_token(a1,lpt,abf) < 0)
					return (-1);
				if (nop == '=') {
					/* 複合代入 `op=` は、右辺の前に置いた
					 * OP_STACKTOPCOPY と合わせ、`x=x op 右辺`
					 * の形にする。 */
					a1.kind = EXP_OPERATOR;
					a1.OPERATOR = '=';
					if (put_token(a1,lpt,abf) < 0)
						return (-1);
				}
				break;
			default:
				if (get_token(p,lpt,abf) < 0)
					return (-1);
				f=1;
				break;
		}
	}
}

/* 内部コード読出。put_token と対になる。容量の検査はしない
 * (put_token の書式どおりに読む)。 */
int get_codetoken(ACUM *acum,int *lpt,ATOM *abf)
{
	acum->kind = abf[(*lpt)++].di;
	if (acum->kind == EXP_TERMINATE ||
		acum->kind == EXP_RESET)
		return (0);

	acum->val = abf[(*lpt)++];
	if (acum->kind >= EXP_VARIABLE) {
		acum->type = abf[(*lpt)++].di;
		if (acum->type & VAR_ARRAY)
			acum->size = abf[(*lpt)++].di;
	}
	return (0);
}

int get_stack(ACUM *acum)
{
	if (stpt <= 0) {
		log_error(ERRM_STACK_EMPTY);
		return (-1);
	}
	*acum = stack[--stpt];
	return (0);
}

int peek_stack(ACUM *acum)
{
	if (stpt <= 0) {
		log_error(ERRM_STACK_EMPTY);
		return (-1);
	}
	*acum = stack[stpt-1];
	return (0);
}

int put_stack(ACUM acum)
{
	if (stpt >= MAX_STACKS) {
		log_error(ERRM_STACK_OVERFLOW);
		return (-1);
	}
	stack[stpt++] = acum;
	return (0);
}

/* スタックから値を1つ取り、変数なら中身を読んで定数化する。
 * 変数の場合、kind は EXP_VARIABLE のまま type だけを
 * EXP_CONST にする。EXP_CONST の値は VAR_CHAR の値と同じ
 * (0x01)ため、値を書き換えて push し直す場合は、呼び出し側で
 * kind を EXP_CONST にし直さないと、共用体の ADDRESS が壊れた
 * まま変数として読まれてしまう（OP_REVERSE はそうしている）。 */
int get_value(ACUM *acum)
{
	if (get_stack(acum)<0)
		return (-1);
	switch (acum->kind) {
		case EXP_CONST:
			break;
		case EXP_VARIABLE:
			if (acum->ADDRESS == nullptr) {
				log_error(ERRM_NULL_REFERENCE);
				return (-1);
			}
			switch (acum->type) {
				case VAR_CHAR:
					acum->VAL = (int) *((char *) acum->ADDRESS);
					break;
				case VAR_SHORT:
					acum->VAL = (int) *((short *) acum->ADDRESS);
					break;
				case VAR_INT:
					acum->VAL = (int) *((int *) acum->ADDRESS);
					break;
				case VAR_LONG:
					acum->VAL = (int) *((long *) acum->ADDRESS);
					break;
				default:
					log_error(ERRM_INVALID_VARTYPE);
					return (-1);
			}
			acum->type = EXP_CONST;
			break;
		default:
			log_error(ERRM_ILLEAGAL_VALUE);
			return (-1);
	}
	return (0);
}

/* 式演算。内部コードを先頭から読み、スタックマシンで実行する。
 * 値は逆ポーランド記法で並んでいるため、演算子はスタックから
 * オペランドを取る。EXP_TERMINATE で終わる。 */
int calc_expression(int n,int *lpt,ATOM *abf)
{
	ACUM a1,a2;
	int old_gp;
	char op;

	old_gp=0;
	while (1) {
		get_codetoken(&a1,lpt,abf);
		if (a1.kind == EXP_TERMINATE)
			break;
		switch (a1.kind) {
			case EXP_RESET:
				stpt=0;
				break;
			case EXP_FUNCTION:
				switch (a1.FUNCTION) {
					case FUNC_RND:
						if (get_value(&a2)<0)
							return (-1);
						a1.kind = EXP_CONST;
						a1.VAL = frand(a2.VAL)+1;
						put_stack(a1);
						break;
					default:
						log_error(ERRM_UNDEF_FUNC);
						return(-1);
				}
				break;
			case EXP_OPERATOR:
				if (a1.OPERATOR == OP_STACKTOPCOPY) {
					if (peek_stack(&a1)<0)
						return (-1);
					if (put_stack(a1)<0)
						return (-1);
					break;
				}
				if(get_value(&a2)<0)
					return(-1);
				op = a1.OPERATOR;
				if (op == OP_REVERSE) {
					a2.VAL = -a2.VAL;
					/* VAL と ADDRESS は共用体を共有するため、
					 * 変数のまま push すると次回参照で壊れた
					 * ポインタを読む。定数として push する
					 * （2026-11-16 修正）。 */
					a2.kind = EXP_CONST;
					if(put_stack(a2)<0)
						return (-1);
					break;
				}
				if (op == '=' ||
					op == '[') {
					/* 代入と添字の左辺は、値を読まずに
					 * 変数のまま取り出す。 */
					if(get_stack(&a1)<0)
						return(-1);
					if (a1.kind != EXP_VARIABLE) {
						log_error(ERRM_ILLEAGAL_SUBST);
						return (-1);
					}
				} else {
					if(get_value(&a1)<0)
						return(-1);
					a1.kind = EXP_CONST;
				}
				switch (op) {
					case '>':
						a1.VAL = (a1.VAL > a2.VAL);
						break;
					case '<':
						a1.VAL = (a1.VAL < a2.VAL);
						break;
					case '!':
						a1.VAL = (a1.VAL != a2.VAL);
						break;
					case OP_GREATEREQUAL:
						a1.VAL = (a1.VAL >= a2.VAL);
						break;
					case OP_LESSEQUAL:
						a1.VAL = (a1.VAL <= a2.VAL);
						break;
					case OP_EQUAL:
						a1.VAL = (a1.VAL == a2.VAL);
						break;
					case '+':
						a1.VAL += a2.VAL;
						break;
					case '-':
						a1.VAL -= a2.VAL;
						break;
					case '*':
						a1.VAL *= a2.VAL;
						break;
					case '/':
						if (a2.VAL == 0) {
							log_error(ERRM_DEVIDE_ZERO);
							return (-1);
						}
						a1.VAL /= a2.VAL;
						break;
					case '%':
						if (a2.VAL == 0) {
							log_error(ERRM_DEVIDE_ZERO);
							return (-1);
						}
						a1.VAL %= a2.VAL;
						break;
					case '&':
						a1.VAL &= a2.VAL;
						break;
					case '|':
						a1.VAL |= a2.VAL;
						break;
					case '^':
						a1.VAL ^= a2.VAL;
						break;
					case '=':
						if (a1.ADDRESS == nullptr) {
							log_error(ERRM_NULL_REFERENCE);
							return (-1);
						}
						switch (a1.type) {
							case VAR_CHAR:
								*(a1.ADDRESS)=(char) a2.VAL;
								break;
							case VAR_SHORT:
								*((short*)a1.ADDRESS)=(short) a2.VAL;
								break;
							case VAR_INT:
								if ((int*)a1.ADDRESS == &(cd[n].gp))
									old_gp = cd[n].gp;
								if ((int*)a1.ADDRESS == &(cd[n].x) ||
									(int*)a1.ADDRESS == &(cd[n].y))
									clear_chr(n);
								*((int*)a1.ADDRESS) = a2.VAL;
								/* 汎用変数・個人変数・保存変数への
								 * 代入は、変わった印として
								 * gfbuf2 へ番号を残す。スイッチ
								 * の実行後にフラグ条件のスイッチ
								 * が動く。 */
								if ((int*)a1.ADDRESS >= generic_variable &&
									(int*)a1.ADDRESS < generic_variable+GVAR_MAX) {
									gfbuf2[gfn2++] =GFLAG_MAX+PFLAG_MAX+((int*)a1.ADDRESS-generic_variable);
								}
								if ((int*)a1.ADDRESS >= cd[n].pvar &&
									(int*)a1.ADDRESS < cd[n].pvar+PVAR_MAX) {
									gfbuf2[gfn2++] =GFLAG_MAX+PFLAG_MAX+GVAR_MAX+((int*)a1.ADDRESS-cd[n].pvar);
								}
								if ((int*)a1.ADDRESS >= cd[n].svar &&
									(int*)a1.ADDRESS < cd[n].svar+SVAR_MAX) {
									gfbuf2[gfn2++] =GFLAG_MAX+PFLAG_MAX+GVAR_MAX+PVAR_MAX+((int*)a1.ADDRESS-cd[n].svar);
								}
								/* 所持金の増減は税に反映する。 */
								if ((int*)a1.ADDRESS == &(cd[n].gp))
									tax += old_gp - cd[n].gp;
								if ((int*)a1.ADDRESS == &(cd[n].x) ||
									(int*)a1.ADDRESS == &(cd[n].y)) {
									set_chr(n);
									print_map(n, 0);
									if (!trnf) {		  /* :O T 指定時には 移動先のスイッチを動作させない */
										cd[n].bx = cd[n].x;
										cd[n].by = cd[n].y;
									}
								}
								break;
							case VAR_LONG:
								*((long*)a1.ADDRESS) = (long) a2.VAL;
								break;
							default:
								/* 対応しない型への代入は、
								 * 記録だけして -1 を返さない。
								 * この後、変数のまま積まれる。
								 * 従来の挙動。 */
								log_error(ERRM_UNSUPORT_SUBST);
								break;
						}
						break;
					case '[':
						if (a1.ADDRESS == nullptr) {
							log_error(ERRM_NULL_REFERENCE);
							return (-1);
						}
						/* 添字は1から数える。符号の混ざった
						 * 比較はそのままにする。負の添字は
						 * 符号なしに広がって大きくなるため、
						 * 前の条件で範囲外になる。 */
						if (a1.size < a2.VAL ||
							1 > a2.VAL) {
							log_error(ERRM_RANGEOUT);
							return (-1);
						}
						a2.VAL--;
						switch (a1.type) {
							case VAR_CHAR_A:
								a1.ADDRESS += a2.VAL;
								break;
							case VAR_SHORT_A:
								a1.ADDRESS=(char*)((short*)a1.ADDRESS+a2.VAL);
								break;
							case VAR_INT_A:
								a1.ADDRESS=(char*)((int*)a1.ADDRESS+a2.VAL);
								break;
							case VAR_LONG_A:
								a1.ADDRESS=(char*)((long*)a1.ADDRESS+a2.VAL);
								break;
							case VAR_OCHR_A:
								a1.ADDRESS=(char*)((CHRData*)a1.ADDRESS+a2.VAL);
								break;
							case VAR_ITEM_A:
								a1.ADDRESS=(char*)((ItemData*)a1.ADDRESS+a2.VAL);
								break;
							case VAR_MAGIC_A:
								a1.ADDRESS=(char*)((MagicData*)a1.ADDRESS+a2.VAL);
								break;
							default:
								log_error(ERRM_UNDEF_ARRAY);
								return (-1);
						}
						a1.type &= ~VAR_ARRAY;
						break;
					default:
						log_error(ERRM_UNDEF_OPERATOR);
						return (-1);
				}
				if (put_stack(a1)<0)
					return(-1);
				break;
			case EXP_ELEMENT:
				if (get_stack(&a2)<0)
					return(-1);
				if (!(a2.kind == EXP_VARIABLE &&
					  a2.type & VAR_OBJECT)) {
					log_error(ERRM_ILLEAGAL_CODE);
					return (-1);
				}
				if (a2.ADDRESS == nullptr) {
					log_error(ERRM_NULL_REFERENCE);
					return (-1);
				}
				a2.type = a1.type;
				a2.ADDRESS += a1.OFFSET;
				a2.size = a1.size;
				if (put_stack(a2)<0)
					return(-1);
				break;
			case EXP_VARIABLE:
				switch (a1.NAME) {
					case 'x':
						a1.ADDRESS = (char*) generic_variable;
						break;
					case 'y':
						a1.ADDRESS = (char*) cd[n].pvar;
						break;
					case 'z':
						a1.ADDRESS = (char*) cd[n].svar;
						break;
					case 'C':
						a1.ADDRESS = (char*) &(cd[n]);
						break;
					case 'i':
						a1.ADDRESS = (char*) &putbuf;
						break;
					case 'w':
						a1.ADDRESS = (char*) cd[n].weq;
						break;
					case 'a':
						a1.ADDRESS = (char*) cd[n].aeq;
						break;
					case 'o':
						a1.ADDRESS = (char*) cd[n].oeq;
						break;
					case 'I':
						a1.ADDRESS = (char*) cd[n].id;
						break;
					case 'm':
						a1.ADDRESS = (char*) &(cd[n].md[0]);
						break;
					case 'B':
						a1.ADDRESS = (char*) cd[n].md;
						break;
					default:
						log_error(ERRM_UNDEF_VAR);
						return (-1);
				}
				if (a1.ADDRESS == nullptr) {
					log_error(ERRM_NULL_REFERENCE);
					return (-1);
				}
				if (put_stack(a1)<0)
					return (-1);
				break;
			case EXP_SYSVAR:
				a1.kind = EXP_CONST;
				switch (a1.NAME) {
					case 'p':
						std::sscanf(cd[n].str,"%i",&(a1.VAL));
						break;
					case 't':
						a1.VAL = tax;
						break;
					case 'T':
						a1.VAL = std::time(nullptr);
						break;
					case 'W':
						a1.VAL = sum_wp(n);
						break;
					case 'A':
						a1.VAL = 0;
						for (int i=0;i<MAX_ITEMS;i++)
							if (cd[n].id[i].type != DT_NONE &&
								cd[n].id[i].ATP > a1.VAL)
								a1.VAL = cd[n].id[i].ATP;
						break;
					case 'D':
						a1.VAL = 0;
						for (int i=0;i<MAX_ITEMS;i++)
							if ((cd[n].id[i].type & IT_MASK) == IT_ARMOR &&
								(cd[n].id[i].DFP + cd[n].id[i].MRP) > a1.VAL)
								a1.VAL = cd[n].id[i].DFP + cd[n].id[i].MRP;
						break;
					case 'm':
						a1.VAL = match_n;
						break;
					case 'w':
						a1.VAL = (cd[n].weq != nullptr);
						break;
					case 'a':
						a1.VAL = (cd[n].aeq != nullptr);
						break;
					case 'o':
						a1.VAL = (cd[n].oeq != nullptr);
						break;
					/* ↓スイッチデータ追加 2005/11/26 Savarl.F */
					case 'P':
						a1.VAL = (n < MAX_NTTYS);	/* is Player Character ? */
						break;
					/* ↑ADD */
					default:
						log_error(ERRM_UNDEF_SYSVAR);
						return (-1);
				}
			/* 参照変数も定数として積む。下の EXP_CONST の
			 * 場合へ落ちる。 */
			case EXP_CONST:
			default:
				if (put_stack(a1)<0)
					return (-1);
				break;
		}
	}
	return(0);
}
