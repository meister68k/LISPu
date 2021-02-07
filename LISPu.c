//
// LISPu : micro LISP
//
// 2021-02-07 NAKAUE,T
// CP/M with z88dk用
//

#include <ctype.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// atom領域の探索
// 引数:
//  atom_pool : atom領域
//  str : 探す文字列
//
// 見つかればatomを示すポインタを返す
// 見つからなければNULLを返す
// 空文字列("")を渡すと領域の末尾が探せる
char* search_atom(char* atom_pool, char* str)
{
    while(strcmp(atom_pool, str) != 0) {
        if(*atom_pool == 0) return NULL;
        atom_pool += strlen(atom_pool) + 1;
    }
    return atom_pool;
}


// atom領域への追加
// 引数:
//  atom_pool : atom領域
//  str : 追加する文字列
char* add_atom(char* atom_pool, char* str)
{
    char* p = search_atom(atom_pool, str);                  // 重複がないか確認
    if(!p) {                            // 未登録だった
        p = search_atom(atom_pool, ""); // atom領域の末尾を取得
        strcpy(p, str);
    }
    return p;
}


// cons cell
// atomの格納用も兼ねる
typedef struct Cell_t {
    struct Cell_t* car;
    struct Cell_t* cdr;                 // atomでは他の型にキャストする
} Cell;

// atomはcar部をATOM_MARKとし，cdr部でデータの実態を指す
#define ATOM_MARK   ((Cell*) 1)

// 数値atomはcar部をATOM_MARKとし，cdr部で16bit整数を指す
#define NUM_MARK    ((Cell*) 2)

// 組込み関数はcar部をFUNC_MARKとし，cdr部で関数へのポインタを指す
#define FUNC_MARK   ((Cell*) 3)

// NIL
// これでいいのか？
#define NIL     NULL


// 組込み関数の型
// 引数0，1つ又は可変引数(0の場合はダミーのparam)
typedef Cell* (*LispFunc)(Cell* env, Cell* param);
// 引数2つ
typedef Cell* (*LispFunc2)(Cell* env, Cell* param1, Cell* param2);


// 未使用のcons cellを一つ取得する
// 引数:
//  env : 環境
// 空きがなければNULLを返す
Cell* getcell(Cell* env)
{
    Cell* cell = (env[1]).car;
    (env[1]).car = cell->cdr;
    return cell;
}


// cons cellを作る
// 引数:
//  env : 環境
//  car_part : car部
//  cdr_part : cdr部
Cell* cons(Cell* env, Cell* car_part, Cell* cdr_part)
{
    Cell* cell = getcell(env);
    cell->car = car_part;
    cell->cdr = cdr_part;
    return cell;
}


// atomかどうか判定(数値を含む)
bool is_atom(Cell* cell)
{
    return ((cell == NULL) || (cell->car == ATOM_MARK) || (cell->car == NUM_MARK));
}


// 数値かどうか判定
bool is_num(Cell* cell)
{
    return (cell->car == NUM_MARK);
}


// 組込み関数かどうか判定
bool is_func(Cell* cell)
{
    return (cell->car == FUNC_MARK);
}


// atomから文字列へのキャスト
#define atom_to_str(cell)   (cell ? (char*) cell->cdr : "nil")


// atomから数値へのキャスト
#define atom_to_int(cell)   (cell ? (intptr_t) cell->cdr : 0)


// atomの比較
bool eqv(Cell* env, Cell* left, Cell* right)
{
    if(left == NULL) return (right == NULL) ? true : false;
    if((left->car == right->car) && (left->cdr == right->cdr)) {
        if(is_atom(left)) return true;  // atom以外もtrueとする？
    }
    return false;
}


// S式の表示
Cell* prnt(Cell* env, Cell* lst);


// S式の表示(cdr部)
void prcdr(Cell* env, Cell* lst)
{
    if(lst == NULL) {
        putchar(')');
    } else if(is_atom(lst)) {
        if(!is_num(lst)) {
            printf(".%s)", atom_to_str(lst));
        } else {
            printf(".%d)", atom_to_int(lst));
        }
    } else {
        putchar(' ');
        prnt(env, lst->car);
        prcdr(env, lst->cdr);
    }
}


// S式の表示
Cell* prnt(Cell* env, Cell* lst)
{
    if(is_atom(lst)) {
        if(!is_num(lst)) {
            printf("%s", atom_to_str(lst));
        } else {
            printf("%d", atom_to_int(lst));
        }
    } else {
        putchar('(');
        prnt(env, lst->car);
        prcdr(env, lst->cdr);
    }

    return NULL;
}


// 空白の読み飛ばし
char* skipspc(char* str)
{
    while((*str != '\0') && (*str <= ' ')) str++;
    return str;
}


// atomの読み飛ばし
char* skipatom(char* str)
{
    while((*str != '\0') && (*str != '(') && (*str != '.') && (*str != ')') && (*str > ' ')) str++;
    return str;
}


// 文字列からatomの生成
Cell* mkatom(Cell* env, char* str)
{
    if(isdigit(*str)) {
        // 数値の可能性あり
        char* str_end = str;
        long num = strtol(str, &str_end, 0);
        if(*str_end == '\0') return cons(env, NUM_MARK, (Cell*) num);
        // 数値として解釈できなかった
        // エラー処理が未完なので，とりあえずatomにしてしまう
    }

    char* atom_pool = (env[2]).car;
    return cons(env, ATOM_MARK, add_atom(atom_pool, str));
}


// 文字列からatomの生成
// 文字列先頭から空白までをatomとして登録する
// 一時的に元の文字列領域に書き込みを行う
Cell* mkatoml(Cell* env, char** str)
{
    char *p = *str;
    *str = skipatom(*str);

    char c = **str;
    **str = '\0';   // strcmpのために一時的に\0を書き込む
    Cell* result = mkatom(env, p);
    **str = c;      // 戻す

    return result;
}


// 組込み関数の登録用セルの生成
Cell* mkfunc(Cell* env, LispFunc func, void* info)
{
    return cons(env, FUNC_MARK, cons(env, (Cell*) func, (Cell*) info));
}


// S式の入力
Cell* read_s(Cell* env, char** str);


// S式の入力(cdr部)
Cell* rdcdr(Cell* env, char** str)
{
    *str = skipspc(*str);
    if(**str == ')') {
        (*str)++;
        return NIL;
    } else if(**str == '.') {
        (*str)++;
        Cell* result = read_s(env, str);
        while((**str != '\0') && (**str != ')')) (*str)++;
        return result;
    }

    return cons(env, read_s(env, str), rdcdr(env, str));
}


// S式の入力
Cell* read_s(Cell* env, char** str)
{
    *str = skipspc(*str);
    if(**str == '\'') {
        (*str)++;
        return cons(env, mkatom(env, "'"), read_s(env, str));
    } else if(**str == '(') {
        (*str)++;
        return rdcdr(env, str);
    }
    return mkatoml(env, str);
}


// 組込み関数read
Cell* lisp_read(Cell* env, Cell* dummy)
{
    puts("READ");
    static char buff[256];
    char* str = fgets(buff, sizeof(buff), stdin);

    return str ? read_s(env, &str) : NULL;
}


// 連想配列の検索
Cell* assq(Cell* env, Cell* key, Cell* dict)
{
    while(dict) {
        Cell* item = dict->car;
        if(eqv(env, key, item->car)) return item;
        dict = dict->cdr;
    }
    return NULL;
}


// 変数リストの取得
#define var_list(env)   (env)


// 変数定義
Cell* define(Cell* env, Cell* name, Cell* val)
{
    Cell* var = cons(env, name, val);
    var_list(env)->car = cons(env, var, var_list(env)->car);
    return name;
}


// 組込み関数登録
Cell* define_func(Cell* env, char* name, LispFunc func, void* info)
{
    return define(env, mkatom(env, name), mkfunc(env, func, info));
}




// 変数参照
Cell* apply(Cell* env, Cell* name)
{
    if(is_num(name)) return name;       // 数値はそのまま返す
    Cell* var = assq(env, name, var_list(env)->car)->cdr;
    return var;
}


// リストの先頭要素を取得
Cell* car(Cell* env, Cell* lst)
{
    return lst->car;
}


// リストの残り要素を取得
Cell* cdr(Cell* env, Cell* lst)
{
    return lst->cdr;
}


#define T_ATOM(env)         ((env[3]).car)
#define LAMBDA_ATOM(env)    ((env[4]).car)


// atomかどうか判定
Cell* atomp(Cell* env, Cell* lst)
{
    return is_atom(lst) ? T_ATOM(env) : NULL;
}


// 同一のatomかどうか判定
Cell* eqp(Cell* env, Cell* left, Cell* right)
{
    return eqv(env, left, right) ? T_ATOM(env) : NULL;
}


Cell* eval(Cell* env, Cell* lst);


// リストをそのまま返す
// (特殊形式)
Cell* quote(Cell* env, Cell* param, Cell* func_)
{
    if(!func_) return param->car;

    LispFunc func = (LispFunc) func_;
    return func(env, param->car);
}


// リストをそのまま返す
// (特殊形式)
Cell* quoteN(Cell* env, Cell* param, Cell* func_)
{
    if(!func_) return param;

    LispFunc func = (LispFunc) func_;
    return func(env, param);
}


// 条件実行
// (特殊形式)
Cell* cond(Cell* env, Cell* param, Cell* func_)
{
    // 引数を順次評価する
    for(Cell* p = param; p; p = p->cdr) {
        if(p->car && p->car->car) {
            Cell *flag = eval(env, p->car->car);
            if(flag) return eval(env, p->car->cdr->car);
        }
    }

    return NULL;
}


// 引数0の組込み関数を呼ぶ
// (特殊形式)
Cell* func_arg0(Cell* env, Cell* param, Cell* func_)
{
    // 引数の個数チェック
    if(param && (param->car != NULL)) {
        // エラー
        puts("Error: %s: needs 0 argument");
        return NULL;
    }

    LispFunc func = (LispFunc) func_;
    return func(env, NULL);
}


// 引数1つの組込み関数を呼ぶ
// (特殊形式)
Cell* func_arg1(Cell* env, Cell* param, Cell* func_)
{
    // 引数の個数チェック
    if(!param || !param->car || (param->cdr != NULL)) {
        // エラー
        puts("Error: %s: needs 1 argument");
        return NULL;
    }

    LispFunc func = (LispFunc) func_;
    return func(env, eval(env, param->car));
}


// 引数2つの組込み関数を呼ぶ
// (特殊形式)
Cell* func_arg2(Cell* env, Cell* param, Cell* func_)
{
    // 引数の個数チェック
    if(!param || !param->car || !param->cdr || (param->cdr->cdr != NULL)) {
        // エラー
        puts("Error: %s: needs 2 argument");
        return NULL;
    }

    LispFunc2 func = (LispFunc2) func_;
    return func(env, eval(env, param->car), eval(env, param->cdr->car));
}


// 引数N個の組込み関数を呼ぶ
// (特殊形式)
Cell* func_argN(Cell* env, Cell* param, Cell* func_)
{
    // 全ての引数を評価する
    for(Cell* p = param; p; p = p->cdr) {
        if(p->car) p->car = eval(env, p->car);
    }
    if(!func_) return param;

    LispFunc func = (LispFunc) func_;
    return func(env, param);
}


// リストを束ねる
Cell* zip(Cell* env, Cell* left, Cell* right)
{
    Cell tmp = {NULL, NULL};
    Cell *p = &tmp;
    while(left && right) {
        p->cdr = cons(env, cons(env, left->car, right->car), NULL);
        p = p->cdr;
        left = left->cdr;
        right = right->cdr;
    }
    return tmp.cdr;
}


// リストを破壊的に接続する
Cell* nconc(Cell* env, Cell* left, Cell* right)
{
    if(!left) return right;
    Cell* p = left;
    while(p->cdr) p = p->cdr;
    p->cdr = right;
    return left;
}


// 評価
Cell* eval(Cell* env, Cell* lst)
{
    if(is_atom(lst)) {
        // 単体のatom
        return apply(env, lst);
    } else {
        // 関数呼び出し
        Cell* func_cell = lst->car;

        if(is_atom(lst->car)) {
            // 通常の関数(ラムダ式直書きではない)
            func_cell = apply(env, lst->car);
        }

        if(is_func(func_cell)) {
            // 組込み関数
            // 組込み関数の入口をすべて特殊形式とし
            // 引数評価する場合はヘルパ関数を経由する
            LispFunc2 func = (LispFunc2) func_cell->cdr->car;
            Cell* info = func_cell->cdr->cdr;
            return func(env, lst->cdr, info);
        } else if(eqv(env, func_cell->car, LAMBDA_ATOM(env))) {
            // ラムダ式
            Cell* lambda = func_cell->cdr;
            Cell* args = lambda->car;
            Cell* func = lambda->cdr->car;

            // 引数を評価
            Cell* vals = func_argN(env, lst->cdr, NULL);

            // ローカル変数に引数を束縛
            Cell* local_var_list = zip(env, args, vals);
            Cell* all_var_list = nconc(env, local_var_list, var_list(env)->car);

            // ローカル環境
            // 先頭にある変数リスト以外はグローバル環境と共有
            Cell* local_env = cons(env, all_var_list, &env[1]);

            return eval(local_env, func);
        }

        puts("illiegal function");
        prnt(env, lst->car);
        puts("\n^^^^^");
        prnt(env, func_cell);
        puts("\n^^^^^");
    }
    return NULL;
}


// 文字列からの評価
Cell* eval_str(Cell* env, char* str)
{
    char* p = str;
    Cell* lst = read_s(env, &p);

    putchar('>');
    prnt(env, lst);
    puts("");

    return eval(env, lst);
}


int main(void)
{
    puts("LISPu\n");

    // メモリ確保
    // atom領域
    char* atom_pool = (char*) calloc(4096, sizeof(char));
    // cell領域
    Cell* cell_pool = (Cell*) calloc(2048, sizeof(Cell));
    printf("atom_pool addr : %X\n", atom_pool);
    printf("cell_pool addr : %X\n", cell_pool);
    puts("");

    // 未使用セルの初期化，全てつないで1本のリストにする
    Cell* cell_pool_last = &cell_pool[2047];
    for(Cell* p = cell_pool; p < cell_pool_last; ) p->cdr = (++p);

    // 実行環境を構築する
    // (未使用cellのリスト atom領域 変数リスト)
    Cell* env = cell_pool;
    (env[0]).car = NULL;                // 変数リスト(最初はNIL)
    (env[1]).car = &(cell_pool[5]);     // 未使用cellのリスト
    (env[2]).car = (Cell*) atom_pool;   // atom領域
    (env[3]).car = mkatom(env, "#t");   // #tの置き場
    (env[4]).car = mkatom(env, "->");   // ->(lambda)の置き場
    (env[4]).cdr = NULL;

    // 組込み定数の定義
    define(env, mkatom(env, "nil"), NULL);
    define(env, T_ATOM(env), T_ATOM(env));                  // T
    define(env, mkatom(env, "'"), NULL);                    // quote
    define(env, LAMBDA_ATOM(env), LAMBDA_ATOM(env));        // ->

    // 組込み関数の定義
    // define_func(env, 名前, ヘルパ関数, 本体)
    define_func(env, "'",       quoteN,     NULL);
    define_func(env, "quote",   quote,      NULL);
    define_func(env, "atom?",   func_arg1,  atomp);
    define_func(env, "eq?",     func_arg2,  eqp);
    define_func(env, "car",     func_arg1,  car);
    define_func(env, "cdr",     func_arg1,  cdr);
    define_func(env, "cons",    func_arg2,  cons);
    define_func(env, "read",    func_arg0,  lisp_read);
    define_func(env, "eval",    func_arg1,  eval);
    define_func(env, "prnt",    func_arg1,  prnt);
    define_func(env, "def",     func_arg2,  define);
    define_func(env, "zip",     func_arg2,  zip);
    define_func(env, "cond",    cond,       NULL);


    // テスト
    prnt(env, eval_str(env, "(def 'second '(-> (x) (car (cdr x))))"));
    putchar('\n');
    prnt(env, eval_str(env, "(def 'third '(-> (x) (car (cdr (cdr x)))))"));
    putchar('\n');
    prnt(env, eval_str(env, "(third (cdr '(4 5 6 7 8)))"));
    putchar('\n');
    prnt(env, eval_str(env, "(cond (nil (prnt '1)) (#t (prnt '2)))"));
    putchar('\n');


//    lst = lisp_read(env, NULL);

    return 0;
}
