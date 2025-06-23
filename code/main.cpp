#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <iomanip>
#include <cctype>
#include <cmath>
#include <sstream>
#include <functional>
#include <unordered_map>
#include <unordered_set>

using namespace std;

bool hasEvalError = false; // 用於紀錄是否有錯誤發生
bool isTopLevel = true; // 用來測最頂層
bool haslambda = false; // 究極爆破法 
bool verboseMode = true; // 忘了做== 

// S-expression type
enum class SExpType {
    SYMBOL, INT, FLOAT, STRING, NIL, T, DOT, LEFT_PAREN, RIGHT_PAREN, QUOTE, PAIR, EOF_TOKEN, PRIMITIVE_PROC, SYSTEM_MESSAGE, USER_PROC, ERROR
};

// S-expression object
class SExp {
public:
    SExpType type;
    string symbolVal;
    int intVal;
    double floatVal;
    string stringVal;
    shared_ptr<SExp> car;
    shared_ptr<SExp> cdr;

    function<shared_ptr<SExp>(vector<shared_ptr<SExp>>&)> procValue; // 存實際的函數
    string procName; // 存函數名(印error用)

    //proj3
    vector<string> params;             
    vector<shared_ptr<SExp>> body;     
    shared_ptr<unordered_map<string, shared_ptr<SExp>>> env; 

    SExp() : type(SExpType::NIL) {}
    SExp(SExpType t) : type(t) {}
    SExp(int val) : type(SExpType::INT), intVal(val) {}
    SExp(double val) : type(SExpType::FLOAT), floatVal(val) {}
    SExp(string val, SExpType t) {
        type = t;
        if (t == SExpType::SYMBOL) {
            symbolVal = val;
        } 
		else if (t == SExpType::STRING) {
            stringVal = val;
        }
    }
    SExp(shared_ptr<SExp> carVal, shared_ptr<SExp> cdrVal) 
        : type(SExpType::PAIR), car(carVal), cdr(cdrVal) {}

    SExp(string name, function<shared_ptr<SExp>(vector<shared_ptr<SExp>>&)> proc) 
        : type(SExpType::PRIMITIVE_PROC), procValue(proc), procName(name) {}

    SExp(vector<string> piyan, vector<shared_ptr<SExp>> b, shared_ptr<unordered_map<string, shared_ptr<SExp>>> e) 
        : type(SExpType::USER_PROC), params(piyan), body(b), env(e) {}    
};

// Token
class Token {
public:
    SExpType type;
    string value;
    int line;
    int column;

    Token() : type(SExpType::NIL), line(1), column(1) {}
    Token(SExpType t, string v, int l, int c) 
        : type(t), value(v), line(l), column(c) {}
};

// Error Problem 
class ParseError : public exception {
public:
    int line;
    int column;
    string tokenValue;
    string expectedMessage;
    bool isNoClosingQuote;
    bool isEOF;

    ParseError(int l, int c, string token, string expected, bool noClosingQuote = false, bool eof = false)
        : line(l), column(c), tokenValue(token), expectedMessage(expected), isNoClosingQuote(noClosingQuote), isEOF(eof) {}
    
    const char* what() const noexcept override { // 自訂報錯類別 
        return "Parse Error";  
    }
};

class EvalError : public exception {
public:
    string errorType; // Error type
    string tokenValue; // Token value
    bool isDivByZero; // flag(a/0)
    
    EvalError(string type, string token, bool divByZero = false) 
        : errorType(type), tokenValue(token), isDivByZero(divByZero) {}
        
    const char* what() const noexcept override { // throw and catch, call e.what()(可得錯誤訊息)
        return "Evaluation Error";
    }
};

// Input 
class InputManager {
private:
    vector<string> lines;      // store input line
    size_t currLine;           // Current Line Index
    size_t currPos;            // Current Column Index
    bool eofReached;           // EOF 
    bool hasUnreadToken;       // has Next S-Exp(first token)?
    Token unreadToken;         // Next S-Exp(first token)
    
    // get one more input line
    void readMoreLines() {
        string line;
        if (getline(cin, line)) {
            lines.push_back(line);
        } 
        else {
            eofReached = true;
        }
    }

    void resetLineNum() {
        currLine = 0;
    }

    void resetColumnNum() {
        currPos = 0;
    }

    void cleanList(){ // remove useless token(only store tokens that have not be processed)
        if (lines.size() > 1) {
            while(lines.size() != 1){
                lines.erase(lines.begin()); // remove first line
            }
        }

        // only one input line left (remove useless tokens in this line)
        if (currPos > 0 && currPos >= lines[0].length()){ // useless tokens finished at the end of the line
            lines.erase(lines.begin()); // remove the only line
        }
        else if (currPos > 0 && currPos < lines[0].length()) { // more useful tokens left in the only line
            lines[0].erase(0, currPos); // remove the tokens that have been processed in the only line
            //cout << lines[0] << ", Length : " << lines[0].length() << ", Column now :" << currPos << endl;
        }
    }

public:
    bool isFisttimeChange = false; // 用於紀錄當前S-Exp剩下的內容是否為全部

    InputManager() : currLine(0), currPos(0), eofReached(false), hasUnreadToken(false) {
        // testnum = trash
        string testLine;
        if (getline(cin, testLine)) {
        } 
        else {
            eofReached = true;
        }
    }
    
    // 獲取當前字符
    char getCurrentChar() {
        if (currLine >= lines.size()) { // 第一次呼叫currLine是0, line is empty, line = 0
            if (!eofReached) { // 初始是False, 這時候currLine = 0, currPos = 0
                readMoreLines(); // 讀一行
                if (currLine >= lines.size()) {
                    return '\0';  // 真的EOF
                }
            } 
            else {
                return '\0';  // EOF
            }
        }
        
        if (currPos >= lines[currLine].length()) {
            return '\n';  // 行尾
        }
        
        return lines[currLine][currPos]; // 讀取當前string的第currPos個字元
    }
    
    // 前進一個字元
    void advance() {
        if (currLine >= lines.size()) {
            if (!eofReached) {
                readMoreLines();
            }
            return;
        }
        
        if (currPos < lines[currLine].length()) {
            currPos++;
        } 
        else {
            // 移動到下一行
            currLine++;
            currPos = 0;
            
            if (currLine >= lines.size() && !eofReached) {
                readMoreLines();
            }
        }
    }
    
    // Real Line Number(Not Index Number)
    int getLineNumber() {
        return currLine + 1;
    }
    
    // Real Column Number(Not Index Number)
    int getColumnNumber() {
        return currPos + 1;
    }

    void resetLine() {
        resetLineNum();
    }

    void resetColumn() {
        resetColumnNum();
    }
    
    bool isEOF() {
        return eofReached && (currLine >= lines.size() || (currLine == lines.size() - 1 && currPos >= lines[currLine].length()));
    }
    
    void skipWhitespace() {
        while (isspace(getCurrentChar())) { // skip 'whitespace' and '\n'
            advance();
        }
    }
    
    void skipComment() {
        while (getCurrentChar() != '\n' && getCurrentChar() != '\0') {
            advance();
        }
        
        if (getCurrentChar() == '\n') {
            advance();  // 跳過換行符
        }
    }
    
    // 判斷是否為分隔符
    bool isSeparator(char c) {
        return isspace(c) || c == '(' || c == ')' || c == '\'' || c == '"' || c == ';' || c == '\0';
    }
    
    // 移動到下一行
    void moveToNextLine() {
        if (currLine < lines.size()) {
            currPos = lines[currLine].length();
            advance();  // 這將移動到下一行
        }
    }
    
    // 保存一個未讀的token
    void ungetToken(Token token) {
        hasUnreadToken = true;
        unreadToken = token;
    }
    
    // 檢查是否有未讀的token
    bool hasToken() {
        return hasUnreadToken;
    }
    
    // 獲取未讀的token
    Token getToken() {
        hasUnreadToken = false;
        return unreadToken;
    }

    // clean lines
    void cleanup() {
        cleanList();
    }

    // look forward from (0, 0)
    bool trashornot(){ 
        if (lines.empty()) return true;
    
        // 存檔當前位置
        int temp = currPos;
    
        // 檢查行中是否只有空白和註解
        string& line = lines[0];
        int i = currPos;
    
        // 跳過空白
        while (i < line.length() && isspace(line[i])) {
            i++;
        }
    
        // 檢查是否為註解或行尾
        bool isTrash = (i >= line.length() || line[i] == ';');
    
        // 還原位置
        currPos = temp;
        return isTrash;
    }

    void trashtoread(){
        if (!lines.empty()) { // 正常到此line就只有存一行input
            lines.erase(lines.begin()); // clean up lines
        }
        
        readMoreLines();
    }
    
    // 獲取當前行的長度
    int getCurrentLineLength() {
        if (currLine < lines.size()) {
            return lines[currLine].length();
        }
        return 0;
    }
};

// 分析器
class Lexer {
private:
    InputManager& input;
    
    // 判斷string是否為有效數字
    bool isValidNumber(string str) {
        if (str.empty()) return false;
        
        // special case
        if (str == "+" || str == "-" || str == "+." || str == "-.") {
            return false;
        }
        
        bool hasDigit = false;
        bool hasDecimal = false;
        
        size_t start = 0;
        if (str[0] == '+' || str[0] == '-') {
            start = 1;
        }
        
        for (size_t i = start; i < str.length(); i++) {
            if (isdigit(str[i])) {
                hasDigit = true;
            } 
			else if (str[i] == '.' && !hasDecimal) {
                hasDecimal = true;
            } 
			else {
                return false;
            }
        }
        
        return hasDigit;
    }
    
    // 處理string中的轉義
    string processEscapeSequences(string str) {
        string result;
        
        for (int i = 0; i < str.length(); i++) {
            if (str[i] == '\\' && i + 1 < str.length()) {
                char next = str[i + 1];
                if (next == 'n' || next == 't' || next == '\\' || next == '"') {
                    // 特殊轉義字元
                    switch (next) {
                        case 'n': result += '\n'; break;
                        case 't': result += '\t'; break;
                        case '\\': result += '\\'; break;
                        case '"': result += '"'; break;
                    }
                    
                    i++; // 跳過轉義字元後的字元
                }
				else {
                    // 非特殊轉義(保留原始反斜縣) 
                    result += '\\';
                }
            }
			else {
                result += str[i];
            }
        }
        
        return result;
    }
    
public:
    Lexer(InputManager& mgr) : input(mgr) {}
    
    // 抓下一個token
    Token getNextToken() {
        // 檢查是否有被預讀掉的token
        if (input.hasToken()) {
            return input.getToken();
        }
        
        // step1：跳過空白和註解
        while (true) {
            input.skipWhitespace();
            
            if (input.getCurrentChar() == ';') {
                input.skipComment();
            } 
            else {
                break;
            }
        }
        
        // 檢查是否到達EOF
        if (input.isEOF()) {
            return Token(SExpType::EOF_TOKEN, "EOF", input.getLineNumber(), input.getColumnNumber());
        }
        
        // step2：判別token
        // 記第一個字元的位置並讀進去c
        // 這裡算的是真正的line和column而不是在lines中的位置
        int startLine = input.getLineNumber();  // return currLine + 1
        int startCol = input.getColumnNumber(); // return Pros + 1
        // 因初始化都是0

        char c = input.getCurrentChar(); // 讀取當前字原 
        
        // 特殊字處理
        if (c == '(') {
            input.advance();
            return Token(SExpType::LEFT_PAREN, "(", startLine, startCol);
        } 
        else if (c == ')') {
            input.advance();
            return Token(SExpType::RIGHT_PAREN, ")", startLine, startCol);
        } 
        else if (c == '\'') {
            input.advance();
            return Token(SExpType::QUOTE, "'", startLine, startCol);
        } 
        else if (c == '"') {
            input.advance();  // 跳過開頭的雙引號
            c = input.getCurrentChar();
            
            // 先檢查是不是合理空字串 
            if (c == '"') {
                input.advance();  // 跳過結尾的雙引號
                return Token(SExpType::STRING, "Empty_String", startLine, startCol); // 合法空字串
            }
            
            string rawValue;  // 未處理轉義的原始string
            bool isEscaping = false;  // 當前是否處於轉義狀態
            
            while (true) {
                c = input.getCurrentChar();
                
                if (c == '\0') {
                    // EOF但字符串未關閉
                    return Token(SExpType::STRING, "", input.getLineNumber(), input.getColumnNumber() );
                } 
				else if (c == '\n' && !isEscaping) {
                    // 行尾但字符串未關閉
                    return Token(SExpType::STRING, "", input.getLineNumber(), input.getCurrentLineLength() + 1);
                }
                
                if (isEscaping) {
                    // 已在轉義狀態(無論下一個字是什麼都直接接受) 
                    rawValue += '\\';  // 保留轉義標記
                    rawValue += c;
                    isEscaping = false;
                } 
				else if (c == '\\') {
                    // 進入轉義狀態(不立即加字) 
                    isEscaping = true;
                }
				else if (c == '"' && !isEscaping) {
                    // 非轉義的雙引號表示string結束
                    input.advance();  // 跳過結尾的雙引號
                    return Token(SExpType::STRING, processEscapeSequences(rawValue), startLine, startCol);
                }
				else {
                    // 普通字
                    rawValue += c;
                }
                
                input.advance();
            }
        } 
        else {
            // 讀取符號或數字
            string value;
            
            while (!input.isSeparator(input.getCurrentChar())) {
                value = value + input.getCurrentChar();
                input.advance();
            }
            
            // 做完到這 column index 對向在本S-Exp後的下一個字元位置 

            // 特殊符號處理
            if (value == "nil" || value == "#f" || value == "()") {
                return Token(SExpType::NIL, "nil", startLine, startCol);
            } 
            else if (value == "t" || value == "#t") {
                return Token(SExpType::T, "#t", startLine, startCol);
            } 
            else if (value == ".") {
                return Token(SExpType::DOT, ".", startLine, startCol);
            }
            
            // 判斷是否為數字類 
            if (isValidNumber(value)) {
                if (value.find('.') != string::npos) {
                    return Token(SExpType::FLOAT, value, startLine, startCol);
                } 
                else {
                    return Token(SExpType::INT, value, startLine, startCol);
                }
            }
            
            // 預設為SYMBOL 
            return Token(SExpType::SYMBOL, value, startLine, startCol);
        }
    }
};

// 語法分析器
class Parser {
private:
    Lexer& lexer;
    InputManager& input;
    Token currentToken;
    bool hasError;
    string errorMessage;
    
    // 解析第一條文法 
    shared_ptr<SExp> parseAtom() {
        switch (currentToken.type) {
        
            case SExpType::INT: {
                try {
                    int value = stoi(currentToken.value);
                    Token saved = currentToken;
                    //currentToken = lexer.getNextToken();
                    return make_shared<SExp>(value);
                } 
				catch (exception e) {
                    string value = currentToken.value;
                    //currentToken = lexer.getNextToken();
                    return make_shared<SExp>(value, SExpType::SYMBOL);
                }
            }
            case SExpType::FLOAT: {
                try {
                    double value = stod(currentToken.value);
                    //currentToken = lexer.getNextToken();
                    return make_shared<SExp>(value);
                } 
				catch (exception e) {
                    string value = currentToken.value;
                    //currentToken = lexer.getNextToken();
                    return make_shared<SExp>(value, SExpType::SYMBOL);
                }
            }
            case SExpType::STRING: {
                if (currentToken.value.empty() && currentToken.type == SExpType::STRING) {
                    throw ParseError(input.getLineNumber(), input.getColumnNumber(), "", "closing quote", true);
                }
                
                if (currentToken.value == "Empty_String") currentToken.value.clear(); // 這是空字串的情況, 把自己填的拉機清掉 
                
                string value = currentToken.value;
                //currentToken = lexer.getNextToken();
                return make_shared<SExp>(value, SExpType::STRING);
            }
            case SExpType::SYMBOL: {
                string value = currentToken.value; // token內容存入value
                //currentToken = lexer.getNextToken();
                return make_shared<SExp>(value, SExpType::SYMBOL);
            }
            case SExpType::NIL: {
                //currentToken = lexer.getNextToken();
                return make_shared<SExp>(SExpType::NIL);
            }
            case SExpType::T: {
                //currentToken = lexer.getNextToken();
                return make_shared<SExp>(SExpType::T);
            }
            default:
                throw ParseError(input.getLineNumber(), input.getColumnNumber(), currentToken.value, "atom or '('");
        }
    }
    
    // 解析第二條文法
    shared_ptr<SExp> parseList() {
        currentToken = lexer.getNextToken(); // 左括號下一個字元 
        
        // null case
        if (currentToken.type == SExpType::RIGHT_PAREN) {
            //currentToken = lexer.getNextToken();
            return make_shared<SExp>(SExpType::NIL);
        }
        
        // 解析列表第一個元素
        shared_ptr<SExp> first;
        if (currentToken.type == SExpType::LEFT_PAREN) {
            first = parseList();
        } 
        else if (currentToken.type == SExpType::QUOTE) {
            first = parseQuote();
        } 
        else {
            first = parseAtom();
        }
        
        currentToken = lexer.getNextToken();
        
        // 檢查是否為點對表示法
        if (currentToken.type == SExpType::DOT) {
            currentToken = lexer.getNextToken();
            
            if (currentToken.type == SExpType::RIGHT_PAREN) {
                throw ParseError(input.getLineNumber(), input.getColumnNumber(), currentToken.value, "atom or '('");
            }
            
            shared_ptr<SExp> rest;
            if (currentToken.type == SExpType::LEFT_PAREN) {
                rest = parseList();
            } 
            else if (currentToken.type == SExpType::QUOTE) {
                rest = parseQuote();
            } 
            else {
                rest = parseAtom();
            }
            
            currentToken = lexer.getNextToken();
            
            if (currentToken.type != SExpType::RIGHT_PAREN) {
                throw ParseError(input.getLineNumber(), input.getColumnNumber(), currentToken.value, "')'");
            }
            

            //currentToken = lexer.getNextToken(); // 右括號下一個字(預讀用) 
            return make_shared<SExp>(first, rest);
        } 
        else {
            // 標準列表解析
            vector<shared_ptr<SExp>> elements;
            elements.push_back(first);
            
            while (currentToken.type != SExpType::RIGHT_PAREN) {
            	
                if (currentToken.type == SExpType::EOF_TOKEN) {
                    throw ParseError(input.getLineNumber(), input.getColumnNumber(), "EOF", "')'", false, true);
                }
                
                if (currentToken.type == SExpType::DOT) {
                    // 點對表示法
                    currentToken = lexer.getNextToken();
                    
                    shared_ptr<SExp> last;
                    if (currentToken.type == SExpType::LEFT_PAREN) {
                        last = parseList();
                    } 
                    else if (currentToken.type == SExpType::QUOTE) {
                        last = parseQuote();
                    } 
                    else {
                        last = parseAtom();
                    }
                    
                    currentToken = lexer.getNextToken(); 
                    
                    if (currentToken.type != SExpType::RIGHT_PAREN) {
                        throw ParseError(input.getLineNumber(), input.getColumnNumber(), currentToken.value, "')'");
                    }
                    
                    //currentToken = lexer.getNextToken();
                    
                    // 構建點對結構
                    shared_ptr<SExp> result = last;
                    for (int i = elements.size() - 1; i >= 0; i--) {
                        result = make_shared<SExp>(elements[i], result);
                    }
                    
                    return result;
                }
                
                // 解析下一個元素
                shared_ptr<SExp> next;
                if (currentToken.type == SExpType::LEFT_PAREN) {
                    next = parseList();
                } 
                else if (currentToken.type == SExpType::QUOTE) {
                    next = parseQuote();
                } 
                else {
                    next = parseAtom();
                }
                
                currentToken = lexer.getNextToken();
                
                elements.push_back(next);
            }
            
            // 右括號下一個字 
            // currentToken = lexer.getNextToken();
            
            // 構建普通列表
            shared_ptr<SExp> result = make_shared<SExp>(SExpType::NIL);
            for (int i = elements.size() - 1; i >= 0; i--) {
                result = make_shared<SExp>(elements[i], result);
            }
            
            return result;
        }
    }
    
    // 解析第三條文法 
    shared_ptr<SExp> parseQuote() {
        currentToken = lexer.getNextToken(); // 引號下一個字 
        
        // 檢查是否遇到 EOF
        if (currentToken.type == SExpType::EOF_TOKEN) {
            throw ParseError(input.getLineNumber(), input.getColumnNumber(), "EOF", "", false, true); // 標記為 isEOF=true 的錯誤 
        }
        
        // 解析引用的表達式
        shared_ptr<SExp> quoted;
        if (currentToken.type == SExpType::LEFT_PAREN) {
            quoted = parseList();
        } 
		else if (currentToken.type == SExpType::QUOTE) {
            quoted = parseQuote();
        } 
		else {
            quoted = parseAtom();
        }
        
        // 構建 (quote <quoted>) 表達式
        shared_ptr<SExp> quoteSymbol = make_shared<SExp>("quote", SExpType::SYMBOL);
        shared_ptr<SExp> nil = make_shared<SExp>(SExpType::NIL);
        shared_ptr<SExp> quotedCons = make_shared<SExp>(quoted, nil);
        
        return make_shared<SExp>(quoteSymbol, quotedCons);
    }
    
public:
    Parser(Lexer& lex, InputManager& mgr) 
        : lexer(lex), input(mgr), hasError(false) {
        currentToken = lexer.getNextToken();
    }
    
    // 解析一個完整的S-Exp
    shared_ptr<SExp> parse() {
        try {
            // 檢查是否為EOF
            if (currentToken.type == SExpType::EOF_TOKEN) {
                throw ParseError(input.getLineNumber(), input.getColumnNumber(), "EOF", "", false, true);
            }
            
            // 根據目前token類型選解析方法
            shared_ptr<SExp> result;
            if (currentToken.type == SExpType::LEFT_PAREN) { // 文法第二條
                result = parseList();
            } 
            else if (currentToken.type == SExpType::QUOTE) { // 文法第三條
                result = parseQuote();
            } 
            else { // 文法第一條
                result = parseAtom();
            }           
            
            //cout << "Befor !!! Line : " << input.getLineNumber() << ", Column Index : " << input.getColumnNumber()-1 << endl;
            // 此處的line為實際input中的行數, column為當前S-Exp的下一個字元欄位數
            input.cleanup(); // 存input的vector清空到剩未處裡完的內容
            input.resetLine(); // 重設行號 = 0
            input.resetColumn(); // 重設列號 = 0
            // 先檢查剩餘的當前行還有無完全由 whitespace + tab + 註解組成的內容
            if (input.trashornot()){ // true就是都垃圾要直接清空內容去讀下一行進來(因該行會不計行數)
                input.trashtoread(); // 讀一行
            }
  
            // 預讀下一個token
            currentToken = lexer.getNextToken();
            // 保存當前token以便下次使用
            input.ungetToken(currentToken);
            
            return result;
        } 
		catch (ParseError e) {
            hasError = true;
            
            if (e.isNoClosingQuote) {
                errorMessage = "ERROR (no closing quote) : END-OF-LINE encountered at Line " + to_string(e.line) + " Column " + to_string(e.column);
                input.cleanup();
                input.resetLine(); // 重設行號 = 0
                input.resetColumn(); // 重設列號 = 0
                input.trashtoread();
                currentToken = lexer.getNextToken();
                input.ungetToken(currentToken);
            } 
			else if (e.isEOF) {
                errorMessage = "ERROR (no more input) : END-OF-FILE encountered";
            } 
			else {
                errorMessage = "ERROR (unexpected token) : " + e.expectedMessage + " expected when token at Line " + to_string(e.line) + " Column " + to_string(e.column-1) + " is >>" + e.tokenValue + "<<";
                input.cleanup();
                input.resetLine(); // 重設行號 = 0
                input.resetColumn(); // 重設列號 = 0
                input.trashtoread();
                currentToken = lexer.getNextToken();
                input.ungetToken(currentToken);
            }
            
            return nullptr;
        }
    }
    
    bool hasEncounteredError() { 
	    return hasError; 
	}
	
    string getErrorMessage() {
	    return errorMessage; 
	}
    
    // 檢查是否已到達EOF
    bool isAtEOF() {
        return currentToken.type == SExpType::EOF_TOKEN;
    }
};

// S-Exp打印器
class Printer {
public:
    void print(shared_ptr<SExp> sexp) {
        if (!sexp) return;
        
        switch (sexp->type) {
            case SExpType::INT:
                cout << sexp->intVal;
                break;
            case SExpType::FLOAT: {
                //幹四捨五入他媽超好用
                double rounded = round(sexp->floatVal * 1000) / 1000;
                cout << fixed << setprecision(3) << rounded;
                break;
            }
            case SExpType::STRING:
                cout << "\"" << sexp->stringVal << "\"";
                break;
            case SExpType::SYSTEM_MESSAGE: // 系統內部function運作完成訊息  
                cout << sexp->stringVal;  
                break;
            case SExpType::SYMBOL:
                cout << sexp->symbolVal;
                break;
            case SExpType::PRIMITIVE_PROC: // 保留字 
                cout << "#<procedure " << sexp->procName << ">";
                break;
            case SExpType::USER_PROC:  // 保留字 
                cout << "#<procedure " << sexp->procName << ">";
                break;
            case SExpType::NIL:
                cout << "nil";
                break;
            case SExpType::T:
                cout << "#t";
                break;
            case SExpType::PAIR:
                printPair(sexp, 0);
                break;            case SExpType::ERROR:
                cout << sexp->stringVal;
                break;
            default:
                break;
        }
    }

private:
    void printPair(shared_ptr<SExp> pair, int indent) {
        if (!pair) return;
        
        cout << "( ";  // 左括號後面跟一個空格
        
        // 直接處理第一個元素, 不換行
        if (pair->car) {
            if (pair->car->type == SExpType::PAIR) {
                // 如果是列表, 遞迴處理
                printPair(pair->car, indent + 2);
            } else {
                // 其他Atom直接印 
                printAtom(pair->car);
            }
        }
        
        // 處理剩下元素
        shared_ptr<SExp> rest = pair->cdr;
        while (rest && rest->type == SExpType::PAIR) {
            cout << endl;
            for (int i = 0; i < indent + 2; i++) cout << " ";
            
            if (rest->car) {
                if (rest->car->type == SExpType::PAIR) {
                    // 如果是列表就遞迴處理
                    printPair(rest->car, indent + 2);
                } else {
                    // Atom直接印 
                    printAtom(rest->car);
                }
            }
            
            rest = rest->cdr;
        }
        
        // 處理點對表示法的情況
        if (rest && rest->type != SExpType::NIL) {
            cout << endl;
            for (int i = 0; i < indent + 2; i++) cout << " ";
            cout << ".";
            
            cout << endl;
            for (int i = 0; i < indent + 2; i++) cout << " ";
            
            if (rest->type == SExpType::PAIR) {
                printPair(rest, indent + 2);
            } 
			else {
                printAtom(rest);
            }
        }
        
        // 印右括號
        cout << endl;
        for (int i = 0; i < indent; i++) cout << " ";
        cout << ")";
    }
    
    // 印Atom表達式
    void printAtom(shared_ptr<SExp> atom) {
        if (!atom) return;
        
        switch (atom->type) {
            case SExpType::INT:
                cout << atom->intVal;
                break;
            case SExpType::FLOAT: {
            	//幹四捨五入他媽超好用 
                double rounded = round(atom->floatVal * 1000) / 1000;
                cout << fixed << setprecision(3) << rounded;
                break;
            }
            case SExpType::STRING:
                cout << "\"" << atom->stringVal << "\"";
                break;
            case SExpType::SYMBOL:
                cout << atom->symbolVal;
                break;
            case SExpType::PRIMITIVE_PROC:  // 保留字 
                cout << "#<procedure " << atom->procName << ">";
                break;
            case SExpType::USER_PROC:  // 保留字 
                cout << "#<procedure " << atom->procName << ">";
                break;
            case SExpType::NIL:
                cout << "nil";
                break;
            case SExpType::T:
                cout << "#t";
                break;
            case SExpType::ERROR:
                cout << "\"" << atom->stringVal << "\"";
                break;
            default:
                break;
        }
    }
};

// S-Exp執行器 
// !!!要寫的function太多了, 所以把定義寫在class裡面, 實際功能拉到外面寫!!! 
class Evaluator {
private:
    // 環境（查表用）
    unordered_map<string, shared_ptr<SExp>> environment;
    InputManager* globalInput;
    Lexer* globalLexer;
    
    // 錯誤處理
    bool hasError;
    string errorMessage;
    
    // S-Exp轉字串（用於錯誤訊息）
    string exprToString(shared_ptr<SExp> expr);
    
    // 檢查是否為內建原始函數名
    bool isPrimitive(string name);
    
    // 初始化所有原始函數
    void initPrimitives();
    
    // 執行List
    bool evalList(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    
    // eval功能 
    bool evalQuote(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    bool evalDefine(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    bool evalLambda(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    bool evalLet(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    bool evalIf(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    bool evalCond(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    bool evalAnd(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    bool evalOr(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    bool evalBegin(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    
    // 額外檢查 
    bool evalCleanEnv(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    bool evalExit(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    
    // 檢查是否List 
    bool isList(shared_ptr<SExp> expr);
    
    // !!! 純函數式計算宣告成static好了 !!! 
    // 跑兩個S-Exp比較是否結構相等 
    static bool isEqual(shared_ptr<SExp> a, shared_ptr<SExp> b);
    
    // 以下primitive相關 

    // proj4
    static shared_ptr<SExp> primitiveCreateErrorObject(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveErrorObject(vector<shared_ptr<SExp>>& args);
    shared_ptr<SExp> primitiveRead(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveWrite(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveDisplayString(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveNewline(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveSymbolToString(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveNumberToString(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveEval(vector<shared_ptr<SExp>>& args, Evaluator& evaluator);

    bool evalSet(shared_ptr<SExp> expr, shared_ptr<SExp>& result);

    // 建構功能 
    static shared_ptr<SExp> primitiveCons(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveList(vector<shared_ptr<SExp>>& args);
    
    // 取值功能
    static shared_ptr<SExp> primitiveCar(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveCdr(vector<shared_ptr<SExp>>& args);
    
    // 判斷功能
    static shared_ptr<SExp> primitiveAtom(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitivePair(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveIsList(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveNull(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveInteger(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveReal(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveNumber(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveString(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveBoolean(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveSymbol(vector<shared_ptr<SExp>>& args);
    
    // 四則運算
    static shared_ptr<SExp> primitiveAdd(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveSubtract(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveMultiply(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveDivide(vector<shared_ptr<SExp>>& args);
    
    // 檢查功能 
    static shared_ptr<SExp> primitiveNot(vector<shared_ptr<SExp>>& args);
    
    // 比較功能 
    static shared_ptr<SExp> primitiveGreater(vector<shared_ptr<SExp>>& args); // > 
    static shared_ptr<SExp> primitiveGreaterEqual(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveLess(vector<shared_ptr<SExp>>& args); // <
    static shared_ptr<SExp> primitiveLessEqual(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveNumEqual(vector<shared_ptr<SExp>>& args);
    
    // 字串操作功能 
    static shared_ptr<SExp> primitiveStringAppend(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveStringGreater(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveStringLess(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveStringEqual(vector<shared_ptr<SExp>>& args);
    
    // 相等測試
    static shared_ptr<SExp> primitiveEqv(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveEqual(vector<shared_ptr<SExp>>& args);
    
    // 檢查參數數量用
    static void checkArity(string name, vector<shared_ptr<SExp>> args, int expected, bool exact = true);
    
    // 取值（處理INT和FLOAT的轉換）
    static double getNumber(shared_ptr<SExp> arg, string funcName);
    
    // 只好再宣告一個Printer來印ERROR的後方內容 (因原先expr內容沒辦法格式化輸出) 
    static string formatExprForError(shared_ptr<SExp> expr) {
        if (!expr) return "nil";
        
        // 我是天才 
        streambuf* oldCout = cout.rdbuf(); // 暫存原本cout的輸出目標（streambuf）
        stringstream ss; // 字串輸出流 
        cout.rdbuf(ss.rdbuf()); // 用此法把cout的輸出改成寫入ss
        
        // 用Printer把要印的格式弄出來到ss 
        Printer printer;
        printer.print(expr);
        
        cout.rdbuf(oldCout); // 記得把cout的輸出目標設回原本的oldCout, 反正就是恢復原狀 
        
        return ss.str(); // 這不就得到帶有格式化的expr了 
    }

public:
    // 建構子
    Evaluator();
    
    // 起始function 
    bool EvalSExp(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    
    // 符號求值
    bool evalSymbol(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    
    // 清理環境(clean-environment)
    void cleanEnvironment();
    
    // 錯誤處理用 
    string getErrorMessage();
    bool hasEncounteredError();
    void resetError();
    
    // 檢查是否會用到 lambda
    bool willUseLambda(shared_ptr<SExp> expr);
    bool willUseLambdaHelper(shared_ptr<SExp> expr, unordered_set<string>& visited);
    // 加SYSTEM_MESSAGE
    shared_ptr<SExp> createSystemMessage(string message) {
        if (!verboseMode) {
            // 如果非 verbose 模式就回傳 nil 而不是SYSMES 
            return make_shared<SExp>(SExpType::NIL);
        }
        
        shared_ptr<SExp> result = make_shared<SExp>();
        result->type = SExpType::SYSTEM_MESSAGE;
        result->stringVal = message;
        return result;
    }

    void setGlobalInput(InputManager* input, Lexer* lexer) {
        globalInput = input;
        globalLexer = lexer;
    }
};

// 建構子 
Evaluator::Evaluator() : hasError(false) {
    initPrimitives();
}

// 好像沒啥屁用 
string Evaluator::exprToString(shared_ptr<SExp> expr) {
    if (!expr) return "nil";
    
    stringstream ss;
    
    switch (expr->type) {
        case SExpType::INT:
            return to_string(expr->intVal);
        case SExpType::FLOAT:
            return to_string(expr->floatVal);
        case SExpType::STRING:
            return "\"" + expr->stringVal + "\"";
        case SExpType::SYMBOL:
            return expr->symbolVal;
        case SExpType::NIL:
            return "nil";
        case SExpType::T:
            return "#t";
        case SExpType::PRIMITIVE_PROC:
            return "#<procedure " + expr->procName + ">";
        case SExpType::USER_PROC:
            return "#<procedure " + expr->procName + ">";
        case SExpType::PAIR: {
            ss << "( ";
            
            // 處理第一個元素
            if (expr->car) {
                if (expr->car->type == SExpType::SYMBOL) {
                    ss << expr->car->symbolVal;
                } 
				else {
                    ss << exprToString(expr->car);
                }
            }
            
            // 處理剩下元素
            shared_ptr<SExp> rest = expr->cdr;
            while (rest && rest->type == SExpType::PAIR) {
                ss << " ";
                if (rest->car) {
                    ss << exprToString(rest->car);
                }
                
                rest = rest->cdr;
            }
            
            // 處理點對表示法
            if (rest && rest->type != SExpType::NIL) {
                ss << " . " << exprToString(rest);
            }
            
            ss << ")";
            return ss.str();
        }
        default:
            return "原神啟動";
    }
}

// 檢查是否為Primitive Function名
bool Evaluator::isPrimitive(string name) {    static const unordered_set<string> primitives = {
        "cons", "list", "car", "cdr", "atom?", "pair?", "list?", "null?",
        "integer?", "real?", "number?", "string?", "boolean?", "symbol?",
        "+", "-", "*", "/", "not", "and", "or", ">", ">=", "<", "<=", "=",
        "string-append", "string>?", "string<?", "string=?",
        "eqv?", "equal?", "begin", "if", "cond", "quote", "clean-environment", 
        "define", "exit", "let", "lambda", "set!", 
        "create-error-object", "error-object?", "read", "write", 
        "display-string", "newline", "symbol->string", "number->string", "eval",
        "verbose?", "verbose"
    };
    
    return primitives.find(name) != primitives.end();
}

// 初始化所有Primitive Function, 被清掉後會再次加入 
void Evaluator::initPrimitives() {
    // 建構功能 
    environment["cons"] = make_shared<SExp>("cons", primitiveCons);
    environment["list"] = make_shared<SExp>("list", primitiveList);
    
    // 取值功能
    environment["car"] = make_shared<SExp>("car", primitiveCar);
    environment["cdr"] = make_shared<SExp>("cdr", primitiveCdr);
    
    // 判斷功能 
    environment["atom?"] = make_shared<SExp>("atom?", primitiveAtom);
    environment["pair?"] = make_shared<SExp>("pair?", primitivePair);
    environment["list?"] = make_shared<SExp>("list?", primitiveIsList);
    environment["null?"] = make_shared<SExp>("null?", primitiveNull);
    environment["integer?"] = make_shared<SExp>("integer?", primitiveInteger);
    environment["real?"] = make_shared<SExp>("real?", primitiveReal);
    environment["number?"] = make_shared<SExp>("number?", primitiveNumber);
    environment["string?"] = make_shared<SExp>("string?", primitiveString);
    environment["boolean?"] = make_shared<SExp>("boolean?", primitiveBoolean);
    environment["symbol?"] = make_shared<SExp>("symbol?", primitiveSymbol);
    
    // 四則運算 
    environment["+"] = make_shared<SExp>("+", primitiveAdd);
    environment["-"] = make_shared<SExp>("-", primitiveSubtract);
    environment["*"] = make_shared<SExp>("*", primitiveMultiply);
    environment["/"] = make_shared<SExp>("/", primitiveDivide);
    
    // 檢查功能 
    environment["not"] = make_shared<SExp>("not", primitiveNot);
    
    // 比較功能 
    environment[">"] = make_shared<SExp>(">", primitiveGreater);
    environment[">="] = make_shared<SExp>(">=", primitiveGreaterEqual);
    environment["<"] = make_shared<SExp>("<", primitiveLess);
    environment["<="] = make_shared<SExp>("<=", primitiveLessEqual);
    environment["="] = make_shared<SExp>("=", primitiveNumEqual);
    
    // 字串操作功能 
    environment["string-append"] = make_shared<SExp>("string-append", primitiveStringAppend);
    environment["string>?"] = make_shared<SExp>("string>?", primitiveStringGreater);
    environment["string<?"] = make_shared<SExp>("string<?", primitiveStringLess);
    environment["string=?"] = make_shared<SExp>("string=?", primitiveStringEqual);
    
    // 相等測試
    environment["eqv?"] = make_shared<SExp>("eqv?", primitiveEqv);
    environment["equal?"] = make_shared<SExp>("equal?", primitiveEqual);
    
    // exit
    environment["exit"] = make_shared<SExp>("exit", [](vector<shared_ptr<SExp>>& args) -> shared_ptr<SExp> {
        return make_shared<SExp>(SExpType::NIL);
    });

    // proj4 
    environment["create-error-object"] = make_shared<SExp>("create-error-object", primitiveCreateErrorObject);
    environment["error-object?"] = make_shared<SExp>("error-object?", primitiveErrorObject);
    environment["read"] = make_shared<SExp>("read", [this](vector<shared_ptr<SExp>>& args) -> shared_ptr<SExp> {
        return this->primitiveRead(args);
    });
    environment["write"] = make_shared<SExp>("write", primitiveWrite);
    environment["display-string"] = make_shared<SExp>("display-string", primitiveDisplayString);
    environment["newline"] = make_shared<SExp>("newline", primitiveNewline);    environment["symbol->string"] = make_shared<SExp>("symbol->string", primitiveSymbolToString);
    environment["number->string"] = make_shared<SExp>("number->string", primitiveNumberToString);    environment["eval"] = make_shared<SExp>("eval", [this](vector<shared_ptr<SExp>>& args) -> shared_ptr<SExp> {
        checkArity("eval", args, 1);
    
        shared_ptr<SExp> result;
    
        // 保存當前的 top-level 狀態
        bool wasTopLevel = isTopLevel;
        
        // eval 用的表達式應該在top-level執行
        isTopLevel = true;
        
        bool success = this->EvalSExp(args[0], result);
    
        // 恢復 top-level
        isTopLevel = wasTopLevel;
    
        if (!success) {
            // 直接回傳錯誤而不是拋error (避免錯誤包裝) 
            shared_ptr<SExp> errorObj = make_shared<SExp>();
            errorObj->type = SExpType::ERROR;
            errorObj->stringVal = this->getErrorMessage();
            return errorObj;
        }
        
        // 如果結果是SYSMES需立即印 
        if (result && result->type == SExpType::SYSTEM_MESSAGE) {
            Printer printer;
            printer.print(result);
            cout << endl;
        }        
		
		return result;
    });
    
    // verbose (陷阱) 
    environment["verbose?"] = make_shared<SExp>("verbose?", [](vector<shared_ptr<SExp>>& args) -> shared_ptr<SExp> {
        checkArity("verbose?", args, 0);
        return verboseMode ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
    });
    
    environment["verbose"] = make_shared<SExp>("verbose", [](vector<shared_ptr<SExp>>& args) -> shared_ptr<SExp> {
        checkArity("verbose", args, 1);
        verboseMode = (args[0]->type != SExpType::NIL);
        return args[0];
    });
}

// 開始
bool Evaluator::EvalSExp(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    if (!expr) {
        result = make_shared<SExp>(SExpType::NIL);
        return true;
    }
    
    // 存當前level狀態
    bool wasTopLevel = isTopLevel;
    bool res;
    bool currTopLevel;
    
    // 除了top-level外，其他都設為非top-level(抓出在錯誤level執行的功能)
    if (!wasTopLevel) {
        isTopLevel = false;
    }
    
    switch (expr->type) {
        case SExpType::INT:
        case SExpType::FLOAT:
        case SExpType::STRING:
        case SExpType::NIL:
        case SExpType::T:
        case SExpType::PRIMITIVE_PROC:
            // 自求值表達式
            result = expr;
            isTopLevel = wasTopLevel;  // 還原top-level狀態
            return true;
            
        case SExpType::SYMBOL:
            // 查符號綁定
            res = evalSymbol(expr, result);
            // 確保對未綁定符號的錯誤處理是正確的
            if (!res) {
                // 保留原始的 "unbound symbol" 錯誤訊息
                isTopLevel = wasTopLevel;  // 還原top-level狀態
                return false;
            }
            isTopLevel = wasTopLevel;  // 還原top-level狀態
            return res;
            
            
        case SExpType::PAIR:
            // 進入非top-level
            currTopLevel = isTopLevel;
            isTopLevel = false;
            
            // 檢查是否是特殊形式
            if (expr->car && expr->car->type == SExpType::SYMBOL) {
                string op = expr->car->symbolVal;
                if (op == "define" || op == "clean-environment" || op == "exit") { 
                    if (!wasTopLevel) { // 出現在錯誤level 
                        string upperOp;
                        if (op == "define") upperOp = "DEFINE";
                        else if (op == "clean-environment") upperOp = "CLEAN-ENVIRONMENT";
                        else upperOp = "EXIT";
                        
                        errorMessage = "ERROR (level of " + upperOp + ")";
                        hasError = true;
                        isTopLevel = wasTopLevel;  // 還原top-level狀態
                        return false;
                    }
                }
            }
            
            // 評估列表表達式
            res = evalList(expr, result);
            //if (errorMessage.find("ERROR (no return piyan)") != string::npos) return true; 
            //cout << "幹你娘" << errorMessage << endl; 
            // 如果在頂層用失敗且不是特殊形式, 則錯誤訊息轉換為 "no return value"
            // 但不修改已經是特定類型的錯誤訊息
            if (!res && currTopLevel && 
                !(expr->car && expr->car->type == SExpType::SYMBOL && 
                  (expr->car->symbolVal == "define" || expr->car->symbolVal == "clean-environment" || expr->car->symbolVal == "exit"))) {
                //cout << "幹你娘" << errorMessage << endl; 
                // 保留特定類型的錯誤
                if (errorMessage.find("ERROR (unbound symbol)") != string::npos ||
                    errorMessage.find("ERROR (attempt to apply non-function)") != string::npos ||
                    errorMessage.find("ERROR (incorrect number of arguments)") != string::npos ||
                    errorMessage.find("ERROR (car with incorrect argument type)") != string::npos ||
                    errorMessage.find("ERROR (cdr with incorrect argument type)") != string::npos ||
                    errorMessage.find("ERROR (> with incorrect argument type)") != string::npos ||
                    errorMessage.find("ERROR (unbound parameter)") != string::npos ||
                    errorMessage.find("ERROR (LET format)") != string::npos ||   
                    errorMessage.find("ERROR (LAMBDA format)") != string::npos ||
                    errorMessage.find("ERROR (unbound condition)") != string::npos ||  
                    errorMessage.find("ERROR (* with incorrect argument type)") != string::npos ||  
                    errorMessage.find("ERROR (+ with incorrect argument type)") != string::npos) { 
                    
                    // 不做任何修改
                }
				else if (errorMessage.find("ERROR (no return piyan)") != string::npos){
					string target = "ERROR (no return piyan)";
					string replacement = "ERROR (no return value)";
					int pos = errorMessage.find(target);
					errorMessage.replace(pos, target.length(), replacement);
				} 
				else if (errorMessage.find("ERROR (unbound piyan)") != string::npos){
					string target = "ERROR (unbound piyan)";
					string replacement = "ERROR (unbound parameter)";
					int pos = errorMessage.find(target);
					errorMessage.replace(pos, target.length(), replacement);
				} 
				else if (errorMessage.find("ERROR (unbound papapa)") != string::npos){
					string target = "ERROR (unbound papapa)";
					string replacement = "ERROR (unbound parameter)";
					int pos = errorMessage.find(target);
					errorMessage.replace(pos, target.length(), replacement);
				} 
				else {
                    // 將其他錯誤轉換為 "no return value"
                    errorMessage = "ERROR (no return value) : " + formatExprForError(expr) ;
                }
            }
            
            isTopLevel = currTopLevel;  // 還原top-level狀態
            return res;
            
        default:
            isTopLevel = wasTopLevel;  // 還原top-level狀態
            return false;
    }
}

// symbol
bool Evaluator::evalSymbol(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    auto it = environment.find(expr->symbolVal);
    if (it == environment.end()) {
        errorMessage = "ERROR (unbound symbol) : " + expr->symbolVal;
        hasError = true;
        return false;
    }
    
    result = it->second;
    return true;
}

// List
bool Evaluator::evalList(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    // step1 : 檢查S-Exp是否是有效的List 
    if (expr->cdr && expr->cdr->type != SExpType::NIL) {
        // 遍歷cdr線, 確保最後一個cdr是 NIL
        shared_ptr<SExp> current = expr->cdr;
        while (current->type == SExpType::PAIR) {
            current = current->cdr;
        }
        
        // 如果最後一個cdr不是 NIL, 那代表這是一個點對結構, 不是有效List 
        if (current->type != SExpType::NIL) {
            errorMessage = "ERROR (non-list) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
    }

    // step2 : S-Exp符合List結構後再做function檢查
    if (!expr->car) {
        errorMessage = "ERROR (attempt to apply non-function) : " + formatExprForError(nullptr);
        hasError = true;
        return false;
    }
    
    // step3 : 檢查是否是特殊形式
    if (expr->car->type == SExpType::SYMBOL) {
        string symbol = expr->car->symbolVal;
        //cout << "幹你娘" << symbol << endl; 
        if (symbol == "quote") return evalQuote(expr, result);
        else if (symbol == "if") return evalIf(expr, result);
        else if (symbol == "cond") return evalCond(expr, result);
        else if (symbol == "and") return evalAnd(expr, result);
        else if (symbol == "or") return evalOr(expr, result);
        else if (symbol == "begin") return evalBegin(expr, result);
        else if (symbol == "clean-environment") return evalCleanEnv(expr, result);
        else if (symbol == "define") return evalDefine(expr, result);
        else if (symbol == "lambda") return evalLambda(expr, result);
        else if (symbol == "let") return evalLet(expr, result);
        else if (symbol == "exit") return evalExit(expr, result);
        else if (symbol == "set!") return evalSet(expr, result);
    }
    
    // step4 : 求值(對第一個元素進行求值)
    shared_ptr<SExp> op;
    string funcName = "";
    
    if (expr->car->type == SExpType::SYMBOL) {
        funcName = expr->car->symbolVal;
        
        // 檢查有沒有在環境裡 
        auto it = environment.find(expr->car->symbolVal);
        if (it == environment.end()) { // 代表沒這東西 
            errorMessage = "ERROR (unbound symbol) : " + expr->car->symbolVal;
            hasError = true;
            return false;
        }
        
        op = it->second;
    } 
    else {
        // 評估運算符表達式
        if (!EvalSExp(expr->car, op)) {
            errorMessage = "ERROR (no return piyan) : " + formatExprForError(expr->car);
            hasError = true;
            return false;
        }
        
        funcName = exprToString(expr->car);
    }
    
    // step5 : 確認求值後的第一個元素是一個函數(原始函數或用戶定義函數)
    if (op->type != SExpType::PRIMITIVE_PROC && op->type != SExpType::USER_PROC) {
        errorMessage = "ERROR (attempt to apply non-function) : " + formatExprForError(op);
        hasError = true;
        return false;
    }
    
    // step6 : 參數數量檢查(針對所有函數類型)
    int argCount = 0;
    shared_ptr<SExp> argCheck = expr->cdr;
    
    while (argCheck && argCheck->type == SExpType::PAIR) {
        argCount++;
        argCheck = argCheck->cdr;
    }
    
    // 檢查用戶定義函數的參數數量
    if (op->type == SExpType::USER_PROC) {
        if (argCount != op->params.size()) {
            errorMessage = "ERROR (incorrect number of arguments) : " + op->procName;
            hasError = true;
            return false;
        }
    }
    
    // 檢查不同原始函數對應的參數數量是否正確
    if (op->type == SExpType::PRIMITIVE_PROC) {
        if (op->procName == "pair?") {
            if (argCount != 1) {
                errorMessage = "ERROR (incorrect number of arguments) : pair?";
                hasError = true;
                return false;
            }
        } 
        else if (op->procName == "cons") {
            if (argCount != 2) {
                errorMessage = "ERROR (incorrect number of arguments) : cons";
                hasError = true;
                return false;
            }
        }
    }
    
    // step7 : 輪到參數求值 
    vector<shared_ptr<SExp>> args;
    shared_ptr<SExp> argList = expr->cdr;
    
    while (argList && argList->type != SExpType::NIL) {
        // 確保參數列表符合PAIR結構
        if (argList->type != SExpType::PAIR) {
            errorMessage = "ERROR (non-list) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        shared_ptr<SExp> argVal;
        bool oldTopLevel = isTopLevel;
        isTopLevel = false; // 現在不在top-level 
        
        if (!EvalSExp(argList->car, argVal)) { 
            // 參數求值失敗時的處理
            
            // 先檢查是否是重要的錯誤類型要優先保留
            if (errorMessage.find("ERROR (incorrect number of arguments)") != string::npos ||
                errorMessage.find("ERROR (attempt to apply non-function)") != string::npos ||
                errorMessage.find("ERROR (car with incorrect argument type)") != string::npos ||
                errorMessage.find("ERROR (cdr with incorrect argument type)") != string::npos ||
                errorMessage.find("ERROR (> with incorrect argument type)") != string::npos ||
                errorMessage.find("ERROR (< with incorrect argument type)") != string::npos ||
                errorMessage.find("ERROR (>= with incorrect argument type)") != string::npos ||
                errorMessage.find("ERROR (<= with incorrect argument type)") != string::npos ||
                errorMessage.find("ERROR (= with incorrect argument type)") != string::npos ||
                errorMessage.find("ERROR (+ with incorrect argument type)") != string::npos ||
                errorMessage.find("ERROR (- with incorrect argument type)") != string::npos ||
                errorMessage.find("ERROR (* with incorrect argument type)") != string::npos ||
                errorMessage.find("ERROR (/ with incorrect argument type)") != string::npos ||
                errorMessage.find("ERROR (string-append with incorrect argument type)") != string::npos ||
                errorMessage.find("ERROR (string>? with incorrect argument type)") != string::npos ||
                errorMessage.find("ERROR (string<? with incorrect argument type)") != string::npos ||
                errorMessage.find("ERROR (string=? with incorrect argument type)") != string::npos ||
                errorMessage.find("ERROR (division by zero)") != string::npos ||
                errorMessage.find("ERROR (LET format)") != string::npos ||
                errorMessage.find("ERROR (DEFINE format)") != string::npos ||
                errorMessage.find("ERROR (COND format)") != string::npos ||
                errorMessage.find("ERROR (LAMBDA format)") != string::npos ||
                errorMessage.find("ERROR (level of") != string::npos ||
                errorMessage.find("with incorrect argument type") != string::npos ||
                errorMessage.find("ERROR (non-list)") != string::npos ||
                errorMessage.find("ERROR (no return piyan)") != string::npos ||
				errorMessage.find("ERROR (unbound piyan)") != string::npos || 
				errorMessage.find("ERROR (unbound papapa)") != string::npos ) {
                // 保留不做任何修改
            }
            // 然後檢查是否是未綁定符號錯誤
            else if (errorMessage.find("ERROR (unbound symbol)") != string::npos) {
                // 保留 "unbound symbol" 錯誤
            } 
            // 接著處理算術運算函數的參數求值失敗
            else if (expr->car && expr->car->type == SExpType::SYMBOL && 
                (expr->car->symbolVal == "+" || expr->car->symbolVal == "-" || 
                 expr->car->symbolVal == "*" || expr->car->symbolVal == "/")) {
                // 是算術運算函數的參數求值失敗將錯誤轉為 "unbound parameter"
                errorMessage = "ERROR (unbound piyan) : " + formatExprForError(argList->car);
            }
            // 最後處理 "no return value" 錯誤
            else if (errorMessage.find("ERROR (no return value)") != string::npos) {
                //  "no return value" 錯誤轉為 "unbound parameter"
                errorMessage = "ERROR (unbound papapa) : " + formatExprForError(argList->car);
            }
            else {
                // 其他情況轉為 "unbound parameter"
                errorMessage = "ERROR (unbound parameter) : " + formatExprForError(argList->car);
            }
            
            hasError = true;
            isTopLevel = oldTopLevel;
            return false;
        }
        
        isTopLevel = oldTopLevel;
        
        args.push_back(argVal); // 把每個參數求值後的結果存起來 
        argList = argList->cdr; // 下面一位 
    }
      // step8 : 沒報錯的話會有各參數執行結果 
    try {
        // 處理原始函數
        if (op->type == SExpType::PRIMITIVE_PROC) {
            result = op->procValue(args);
            return true;
        }
        // 處理用戶定義函數
        else if (op->type == SExpType::USER_PROC) {
            // 保存當前環境
            
            auto oldEnv = environment;
            
            // 創新環境根據函數類型而定
            unordered_map<string, shared_ptr<SExp>> newEnv;
            
            if (op->env == nullptr) {
                // Lambda：用當前的環境
                
                newEnv = environment;
            }            
            else {
			
                // 普通user定義函數：用函數定義時的環境作為基礎
                newEnv = *(op->env);
                
                // 將當前環境中新定義的函數和變數合併進來
                // 這樣在 let 定義的局部變數和外層新定義的函數都能看到 
                for (auto pair : environment) {
                    // 檢查是否是函數定義時不存在的新項目
                    if (op->env->find(pair.first) == op->env->end()) {
                        // 新定義的函數或變數加到環境中
                        newEnv[pair.first] = pair.second;
                    }
                    // 如果是 let 創的臨時變數也要覆蓋原有的
                    else if (pair.second.get() != op->env->at(pair.first).get()) {
                        // 新的綁定應用新的值
                        newEnv[pair.first] = pair.second;
                    }
                }
            }
            
            // 綁定參數到新環境
            for (int i = 0; i < op->params.size(); i++) {
                newEnv[op->params[i]] = args[i];
            }
            
            // 使用新環境
            environment = newEnv;
            
            // 執行函數體（照 begin）
            shared_ptr<SExp> currentResult;
            bool success = true;
              for (int i = 0; i < op->body.size(); i++) {
                bool isLastExpr = (i == op->body.size() - 1);
                if (EvalSExp(op->body[i], currentResult)) {
                    // 執行成功檢查是否是SYSMES 
                    if (currentResult && currentResult->type == SExpType::SYSTEM_MESSAGE) {
                        // SYSMES需印（除非是最後一個表達式）
                        if (!isLastExpr) {
                            Printer printer;
                            printer.print(currentResult);
                            cout << endl;
                        }
                    }
                    
                    if (isLastExpr) {
                        result = currentResult;
                    }
                }
				else {
                    if (errorMessage.find("ERROR (unbound symbol)") != string::npos ||
                        errorMessage.find("ERROR (attempt to apply non-function)") != string::npos ||
                        errorMessage.find("ERROR (incorrect number of arguments)") != string::npos ||
                        errorMessage.find("ERROR (division by zero)") != string::npos ||
                        errorMessage.find("ERROR (LET format)") != string::npos ||
                        errorMessage.find("ERROR (DEFINE format)") != string::npos ||
                        errorMessage.find("ERROR (COND format)") != string::npos ||
                        errorMessage.find("ERROR (LAMBDA format)") != string::npos ||
                        errorMessage.find("ERROR (level of") != string::npos ||
                        errorMessage.find("with incorrect argument type") != string::npos ||
                        errorMessage.find("ERROR (non-list)") != string::npos ||
                        errorMessage.find("ERROR (unbound parameter)") != string::npos ||
                        //errorMessage.find("ERROR (unbound papapa)") != string::npos ||
                        //errorMessage.find("ERROR (unbound piyan)") != string::npos ||
                        //errorMessage.find("ERROR (no return piyan)") != string::npos ||
                        errorMessage.find("ERROR (unbound test-condition)") != string::npos ||
                        errorMessage.find("ERROR (unbound condition)") != string::npos) {
                        success = false;
                        break;
                    }
                    
                    if (isLastExpr) {
                        success = false;
                        break;
                    }
                }
            }
              // 只恢復函數參數 (保留 set! 和 define 的效果) 
            for (string param : op->params) {
                if (oldEnv.find(param) != oldEnv.end()) {
                    environment[param] = oldEnv[param];
                } 
				else {
                    environment.erase(param);
                }
            }
            
            if (!success) {
                return false;
            }
            
            return true;
        }
    } 
    catch (EvalError e) {
        if (e.isDivByZero) {
            errorMessage = "ERROR (division by zero) : " + e.tokenValue;
        } 
        else {
            errorMessage = "ERROR (" + e.errorType + ") : " + e.tokenValue;
        }
        
        hasError = true;
        return false;
    }
    
    // 如果到這則表示有邏輯錯誤
    return false;
}

// quote
bool Evaluator::evalQuote(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    // 檢查參數數量
    if (!expr->cdr || expr->cdr->type != SExpType::PAIR || !expr->cdr->car || expr->cdr->cdr->type != SExpType::NIL) {
        errorMessage = "ERROR (incorrect number of arguments) : quote";
        hasError = true;
        return false;
    }
    
    // 直接return未判斷的參數
    result = expr->cdr->car;
    return true;
}

// define
bool Evaluator::evalDefine(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    // 檢查參數數量
    if (!expr->cdr || expr->cdr->type != SExpType::PAIR) {
        errorMessage = "ERROR (DEFINE format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    // 檢查定義類型
    if (expr->cdr->car->type == SExpType::SYMBOL) {
        // 語法: (define 符號 值)
        
        // 檢查語法
        if (!expr->cdr->cdr || expr->cdr->cdr->type != SExpType::PAIR || 
            !expr->cdr->cdr->car || expr->cdr->cdr->cdr->type != SExpType::NIL) {
            errorMessage = "ERROR (DEFINE format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
   
        string symbol = expr->cdr->car->symbolVal;
        
        // 檢查是否為保留字
        if (isPrimitive(symbol)) {
            errorMessage = "ERROR (DEFINE format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        // 求值
        shared_ptr<SExp> value;
        if (!EvalSExp(expr->cdr->cdr->car, value)) {
        	if (errorMessage.find("ERROR (unbound papapa)") != string::npos){
        		string target = "ERROR (unbound papapa)";
				string replacement = "ERROR (unbound parameter)";
				int pos = errorMessage.find(target);
				errorMessage.replace(pos, target.length(), replacement);
        		return false;
			}
			
        	errorMessage = "ERROR (no return value) : " + formatExprForError(expr->cdr->cdr->car);
            return false;
        }
        // 加到環境
        environment[symbol] = value;
        
        result = createSystemMessage(symbol + " defined");
        return true;
    }
    else if (expr->cdr->car->type == SExpType::PAIR) {
        // 語法: (define (函數名 參數...) 函數體...)
        // 檢查函數名是否是符號
        if (!expr->cdr->car->car || expr->cdr->car->car->type != SExpType::SYMBOL) {
            errorMessage = "ERROR (DEFINE format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        string funcName = expr->cdr->car->car->symbolVal;
        
        // 檢查是否為保留字
        if (isPrimitive(funcName)) {
            errorMessage = "ERROR (DEFINE format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        // 收集參數
        vector<string> params;
        shared_ptr<SExp> paramsList = expr->cdr->car->cdr;
        while (paramsList && paramsList->type == SExpType::PAIR) {
            if (paramsList->car->type != SExpType::SYMBOL) {
                errorMessage = "ERROR (DEFINE format) : " + formatExprForError(expr);
                hasError = true;
                return false;
            }
            
            params.push_back(paramsList->car->symbolVal);
            paramsList = paramsList->cdr;
        }
        
        // 確保參數列表結尾是 nil
        if (paramsList && paramsList->type != SExpType::NIL) {
            errorMessage = "ERROR (DEFINE format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        // 檢查函數體
        if (!expr->cdr->cdr || expr->cdr->cdr->type == SExpType::NIL) {
            errorMessage = "ERROR (DEFINE format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        // 收集函數體
        vector<shared_ptr<SExp>> body;
        shared_ptr<SExp> bodyList = expr->cdr->cdr;
        while (bodyList && bodyList->type == SExpType::PAIR) {
            body.push_back(bodyList->car);
            bodyList = bodyList->cdr;
        }
        
        // 確保函數體列表結尾是 nil
        if (bodyList && bodyList->type != SExpType::NIL) {
            errorMessage = "ERROR (DEFINE format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        // 創環境副本
        shared_ptr<unordered_map<string, shared_ptr<SExp>>> envCopy = make_shared<unordered_map<string, shared_ptr<SExp>>>(environment);
        
        // 創函數對象
        shared_ptr<SExp> funcObj = make_shared<SExp>(params, body, envCopy);
        funcObj->type = SExpType::USER_PROC;
        funcObj->procName = funcName;
        
        // 加到環境
        environment[funcName] = funcObj;
        
        result = createSystemMessage(funcName + " defined");
        return true;
    }
    else {
        errorMessage = "ERROR (DEFINE format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
}

bool Evaluator::evalLambda(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    // 語法: (lambda (參數列表) 函數體...)
    
    // step1 : 檢查參數部分
    if (!expr->cdr || expr->cdr->type != SExpType::PAIR) {
        errorMessage = "ERROR (LAMBDA format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    // step2 : 檢查參數列表格式
    shared_ptr<SExp> paramsList = expr->cdr->car;
    if (paramsList->type != SExpType::PAIR && paramsList->type != SExpType::NIL) {
        errorMessage = "ERROR (LAMBDA format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    // step3 : 收集參數名
    vector<string> params;
    shared_ptr<SExp> current = paramsList;
    while (current && current->type == SExpType::PAIR) {
        if (current->car->type != SExpType::SYMBOL) {
            errorMessage = "ERROR (LAMBDA format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        params.push_back(current->car->symbolVal);
        current = current->cdr;
    }
    
    // 確保參數列表結尾是 nil
    if (current && current->type != SExpType::NIL) {
        errorMessage = "ERROR (LAMBDA format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    // step4 : 檢查函數體
    shared_ptr<SExp> bodyList = expr->cdr->cdr;
    if (!bodyList || bodyList->type == SExpType::NIL) {
        errorMessage = "ERROR (LAMBDA format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    // step5 : 收集函數體表達式
    vector<shared_ptr<SExp>> body;
    current = bodyList;
    while (current && current->type == SExpType::PAIR) {
        body.push_back(current->car);
        current = current->cdr;
    }
    
    // 確保函數體列表結尾是 nil
    if (current && current->type != SExpType::NIL) {
        errorMessage = "ERROR (LAMBDA format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    // step6 : 不在定義時捕獲環境而是傳 nullptr
    // Lambda 在被用時才會捕獲當時的環境
    shared_ptr<unordered_map<string, shared_ptr<SExp>>> envCopy = nullptr;
    
    // step7 : 創 lambda 函數對象
    result = make_shared<SExp>(params, body, envCopy);
    result->procName = "lambda"; // 用於顯示
    
    return true;
}

bool Evaluator::evalLet(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    // 語法: (let ((變量1 值1) (變量2 值2)...) 表達式...)
    
    // step1 : 檢查有變量綁定和至少一個表達式
    if (!expr->cdr || expr->cdr->type != SExpType::PAIR || 
        !expr->cdr->cdr || expr->cdr->cdr->type == SExpType::NIL) {
        errorMessage = "ERROR (LET format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    // step2 : 綁定列表必須是 PAIR or NIL
    shared_ptr<SExp> bindingsList = expr->cdr->car;
    if (bindingsList->type != SExpType::PAIR && bindingsList->type != SExpType::NIL) {
        errorMessage = "ERROR (LET format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    // step3 : 先檢查所有綁定的格式不求值
    shared_ptr<SExp> current = bindingsList;
    while (current && current->type == SExpType::PAIR) {
        // 每個綁定必須是 PAIR 類型
        if (current->car->type != SExpType::PAIR) {
            errorMessage = "ERROR (LET format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
          // 綁定的第一個元素必須是符號
        if (!current->car->car || current->car->car->type != SExpType::SYMBOL) {
            errorMessage = "ERROR (LET format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
          // 檢查變數名是否是內建函數名（特殊檢查）
        string varName = current->car->car->symbolVal;
        if (environment.find(varName) != environment.end() && environment[varName]->type == SExpType::PRIMITIVE_PROC) {
            // 如果嘗試綁定內建函數名有可能格式錯誤
            // !!!看起來像函數調用時!!!
			// 操 狗東西 
            if (current->car->cdr && current->car->cdr->type == SExpType::PAIR &&
                current->car->cdr->car && current->car->cdr->car->type == SExpType::SYMBOL) {
                errorMessage = "ERROR (LET format) : " + formatExprForError(expr);
                hasError = true;
                return false;
            }
        }
        
        // 綁定必須有值部分
        if (!current->car->cdr || current->car->cdr->type != SExpType::PAIR) {
            errorMessage = "ERROR (LET format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        // 綁定的值之後必須是 NIL（確保每個綁定只有變量名和一個值）
        if (current->car->cdr->cdr && current->car->cdr->cdr->type != SExpType::NIL) {
            errorMessage = "ERROR (LET format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        // 檢查是否有值（防止只有變量名沒有值的情況）
        if (!current->car->cdr->car) {
            errorMessage = "ERROR (LET format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        current = current->cdr;
    }
    
    // 確保綁定列表結尾是 nil
    if (current && current->type != SExpType::NIL) {
        errorMessage = "ERROR (LET format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }    
	
	// step4 : 先在原環境算所有綁定的值
    vector<pair<string, shared_ptr<SExp>>> bindings;
    current = bindingsList;
    
    while (current && current->type == SExpType::PAIR) {
        // 取變量名
        string varName = current->car->car->symbolVal;
        
        // 在原環境中求值所有綁定的值
        shared_ptr<SExp> value;
        bool oldTopLevel = isTopLevel;
        isTopLevel = false; // let 綁定求值不在頂層
        
        if (!EvalSExp(current->car->cdr->car, value)) {
            isTopLevel = oldTopLevel;
            return false;
        }
        
        isTopLevel = oldTopLevel;
        
        // 保存綁定但還不加到環境中
        bindings.push_back(make_pair(varName, value));
        
        current = current->cdr;
    }
    
    // step5 : 創新環境只存區域綁定的變數名
    auto oldEnv = environment;
    vector<string> localVars;
    
    // 將所有計算好的綁定加到環境中並記錄區域變數
    for (auto binding : bindings) {
        localVars.push_back(binding.first);
        environment[binding.first] = binding.second;
    }    
	
	// step6 : 執行 let 主體
    shared_ptr<SExp> body = expr->cdr->cdr;
    shared_ptr<SExp> lastResult;
    bool success = true;
    
    while (body && body->type == SExpType::PAIR) {
        bool isLastExpr = (body->cdr == nullptr || body->cdr->type == SExpType::NIL);
        
        if (EvalSExp(body->car, lastResult)) {
            // 成功執行
            if (isLastExpr) {
                result = lastResult;
            }        
		} 
		else {
            // 執行失敗恢復區域變數但保留 set! 和 define 設的全域變數
            for (string varName : localVars) {
                if (oldEnv.find(varName) != oldEnv.end()) {
                    environment[varName] = oldEnv[varName];
                } 
				else {
                    environment.erase(varName);
                }
            }
            
            return false;
        }
        
        body = body->cdr;
    }    
	
	// 只恢復 let 綁定的區域變數 (保留其他變更) 
    for (string varName : localVars) {
        if (oldEnv.find(varName) != oldEnv.end()) {
            environment[varName] = oldEnv[varName];
        } 
		else {
            environment.erase(varName);
        }
    }
    
    /* 不用恢復其他變數原因：
     1. define 創的函數應保留在全域環境
     2. set! 設的變數應該保留在全域環境
     3. 只有 let 綁定的區域變數需被恢復
    */ 
    
    return success;
}

// if
bool Evaluator::evalIf(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    // 檢查參數
    shared_ptr<SExp> args = expr->cdr;
    
    // step1 : 檢查參數數量是否在允許範圍內
    if (!args || args->type != SExpType::PAIR || !args->cdr || args->cdr->type != SExpType::PAIR) {
        errorMessage = "ERROR (incorrect number of arguments) : if";
        hasError = true;
        return false;
    }
    
    // step2 : 檢查條件和後方式子後是否最多只有一個(偽)else式子 
    shared_ptr<SExp> elseBranch = args->cdr->cdr;
    if (elseBranch && elseBranch->type == SExpType::PAIR) {
        // 檢查else式子之後是否還有更多參數
        if (elseBranch->cdr && elseBranch->cdr->type != SExpType::NIL) {
            errorMessage = "ERROR (incorrect number of arguments) : if";
            hasError = true;
            return false;
        }
    }
    
    // step3 : 判斷條件真偽 
    shared_ptr<SExp> conditionExpr = args->car;
    shared_ptr<SExp> condition;
    
    if (!EvalSExp(conditionExpr, condition)) {
        return false;
    }
    
    // step4 : 根據條件的結果選式子做 
    // 非nil（非#f） = true (#t)
    if (condition->type != SExpType::NIL) { // 條件(#t) 
        return EvalSExp(args->cdr->car, result); // 做式子1 
    } 
	else { // 條件(#f) 
        // 先檢查是否有(偽)else式子 
        if (!elseBranch || elseBranch->type == SExpType::NIL) {
            // 沒有else式子代表整個if的結果是nil 
            errorMessage = "ERROR (no return value) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        return EvalSExp(elseBranch->car, result); // // 做式子2(else部分) 
    }
    
    //cout << "Hello world" << endl;
}

// cond
bool Evaluator::evalCond(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    shared_ptr<SExp> clauses = expr->cdr;
    
    if (!clauses || clauses->type == SExpType::NIL) {
        errorMessage = "ERROR (COND format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    // 處理每個子句
    while (clauses && clauses->type == SExpType::PAIR) {
        shared_ptr<SExp> clause = clauses->car;
        
        // 檢查條件分支格式
        if (!clause || clause->type != SExpType::PAIR) {
            errorMessage = "ERROR (COND format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        shared_ptr<SExp> testResult;
        
        // 處理else
        if (clause->car->type == SExpType::SYMBOL && clause->car->symbolVal == "else" && clauses->cdr->type == SExpType::NIL) {   
            testResult = make_shared<SExp>(SExpType::T);
        } 
        else {
            if (!EvalSExp(clause->car, testResult)) {
                return false;
            }
        }
        
        // 執行子句體（照 begin 邏輯）
        if (testResult->type != SExpType::NIL) {
            shared_ptr<SExp> bodyExpr = clause->cdr;
            
            // 如果沒有子句體就檢查格式
            if (!bodyExpr || bodyExpr->type == SExpType::NIL) {
                errorMessage = "ERROR (COND format) : " + formatExprForError(expr);
                hasError = true;
                return false;
            }
            
            // 執行子句體（照 begin 邏輯）
            shared_ptr<SExp> currentResult;
            
            while (bodyExpr && bodyExpr->type == SExpType::PAIR) {
                // 重置錯誤狀態
                resetError();
                
                if (EvalSExp(bodyExpr->car, currentResult)) {
                    // 成功執行
                    result = currentResult;
                } 
				else {
                    // 執行失敗就檢查錯誤類型
                    
                    // 檢查是否要立即中斷
                    if (errorMessage.find("ERROR (unbound symbol)") != string::npos ||
                        errorMessage.find("ERROR (attempt to apply non-function)") != string::npos ||
                        errorMessage.find("ERROR (incorrect number of arguments)") != string::npos ||
                        errorMessage.find("ERROR (division by zero)") != string::npos ||
                        errorMessage.find("ERROR (LET format)") != string::npos ||
                        errorMessage.find("ERROR (DEFINE format)") != string::npos ||
                        errorMessage.find("ERROR (COND format)") != string::npos ||
                        errorMessage.find("ERROR (LAMBDA format)") != string::npos ||
                        errorMessage.find("ERROR (level of") != string::npos ||
                        errorMessage.find("with incorrect argument type") != string::npos ||
                        errorMessage.find("ERROR (non-list)") != string::npos ||
                        errorMessage.find("ERROR (unbound parameter)") != string::npos ||
                        errorMessage.find("ERROR (unbound test-condition)") != string::npos ||
                        errorMessage.find("ERROR (unbound condition)") != string::npos) {
                        // 立即中斷
                        hasError = true;
                        return false;
                    }
                    
                    // 如果是 "no return value" 就檢查是否為最後一個表達式
                    if (bodyExpr->cdr && bodyExpr->cdr->type != SExpType::NIL) {
                        // 不是最後一個表達式就繼續執行下一個
                        resetError();
                    } 
					else {
                        // 是最後一個表達式且沒有返回值代表全錯 
                        errorMessage = "ERROR (no return value) : " + formatExprForError(expr);
                        hasError = true;
                        return false;
                    }
                }
                
                bodyExpr = bodyExpr->cdr;
                
                if (bodyExpr && bodyExpr->type != SExpType::PAIR && bodyExpr->type != SExpType::NIL) {
                    errorMessage = "ERROR (COND format) : " + formatExprForError(expr);
                    hasError = true;
                    return false;
                }
            }
            
            // 如果能執行到這裡代表所有表達式都成功執行了
            return true;
        }
        
        clauses = clauses->cdr;
    }
    
    // 所有子句都為假
    errorMessage = "ERROR (no return value) : " + formatExprForError(expr);
    hasError = true;
    return false;
}

// and
bool Evaluator::evalAnd(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    shared_ptr<SExp> args = expr->cdr; // 取and後方參數 
    
    // 沒有參數時返回 #f(這裡會爆掉)
    if (!args || args->type == SExpType::NIL) {
        errorMessage = "ERROR (incorrect number of arguments) : and";
        hasError = true;
        return false;
    }
    
    // 檢查每個參數, 發現#f就停 
    while (args && args->type != SExpType::NIL) {
        if (args->type != SExpType::PAIR) {
            errorMessage = "ERROR (non-list) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        if (!EvalSExp(args->car, result)) {
            // 如果是if表達式的條件執行失敗則顯示unbound condition錯誤
            if (args->car->type == SExpType::PAIR && args->car->car && 
                args->car->car->type == SExpType::SYMBOL && args->car->car->symbolVal == "if") {
                errorMessage = "ERROR (unbound condition) : " + formatExprForError(args->car);
                hasError = true;
            }
            return false;
        }
        
        // 檢查結果 
        if (result->type == SExpType::NIL) {
            return true;
        }
        
        // 換下一個參數
        args = args->cdr;
    }
    
    // 所有參數皆#t
    return true;
}

// or
bool Evaluator::evalOr(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    shared_ptr<SExp> args = expr->cdr;
    
    // 沒有參數時返回 #f(這裡會爆掉)
    if (!args || args->type == SExpType::NIL) {
        errorMessage = "ERROR (incorrect number of arguments) : or";
        hasError = true;
        return false;
    }
    
    // 檢查每個參數, 發現#t就停 
    while (args && args->type != SExpType::NIL) {
        if (args->type != SExpType::PAIR) {
            errorMessage = "ERROR (non-list) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        if (!EvalSExp(args->car, result)) {
            return false;
        }
        
        // 檢查結果
        if (result->type != SExpType::NIL) {
            return true;
        }
        
        // 換下一個參數
        args = args->cdr;
    }
    
    // 所有參數皆#f
    result = make_shared<SExp>(SExpType::NIL);
    return true;
}

// begin
bool Evaluator::evalBegin(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    shared_ptr<SExp> args = expr->cdr;
    
    // 沒有參數時返回錯誤
    if (!args || args->type == SExpType::NIL) {
        errorMessage = "ERROR (incorrect number of arguments) : begin";
        hasError = true;
        return false;
    }
    
    shared_ptr<SExp> currentResult;
    
    // 依序執行所有表達式
    while (args && args->type != SExpType::NIL) {
        if (args->type != SExpType::PAIR) {
            errorMessage = "ERROR (non-list) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        // 檢查是否為最後一個表達式
        bool isLastExpr = (args->cdr == nullptr || args->cdr->type == SExpType::NIL);
        
        // 重置錯誤狀態
        resetError();
        
        if (EvalSExp(args->car, currentResult)) {
            // 成功執行
            if (isLastExpr) {
                // 如果是最後一個表達式且成功  
                result = currentResult;
                return true;
            }
            // 如果不是最後一個表達式就繼續執行下一個
        } 
		else {
            // 執行失敗，檢查錯誤類型
            //cout << "幹你娘" <<  errorMessage << endl; 
            // 檢查是否需立即中斷
            if (errorMessage.find("ERROR (unbound symbol)") != string::npos ||
                errorMessage.find("ERROR (attempt to apply non-function)") != string::npos ||
                errorMessage.find("ERROR (incorrect number of arguments)") != string::npos ||
                errorMessage.find("ERROR (division by zero)") != string::npos ||
                errorMessage.find("ERROR (LET format)") != string::npos ||
                errorMessage.find("ERROR (DEFINE format)") != string::npos ||
                errorMessage.find("ERROR (COND format)") != string::npos ||
                errorMessage.find("ERROR (LAMBDA format)") != string::npos ||
                errorMessage.find("ERROR (level of") != string::npos ||
                errorMessage.find("with incorrect argument type") != string::npos ||
                errorMessage.find("ERROR (non-list)") != string::npos ||
                errorMessage.find("ERROR (unbound parameter)") != string::npos ||
                errorMessage.find("ERROR (unbound test-condition)") != string::npos ||
                errorMessage.find("ERROR (unbound condition)") != string::npos) {
                // 立即中斷
                hasError = true;
                return false;
            }
            
            // 如果是 "no return value" 錯誤
            if (isLastExpr) {
            	//cout << "幹你娘" << errorMessage << endl;
                // 如果是最後一個表達式失敗代表全錯 
                errorMessage = "ERROR (no return value) : " + formatExprForError(expr);
                hasError = true;
                return false;
            }
            // 如果不是最後一個表達式就忽略 "no return value" 繼續執行下一個表達式
            resetError();
        }
        
        // 換下一個表達式
        args = args->cdr;
    }
    
    // 如果到達這裡代表沒有表達式被執行（理論上不會發生）
    errorMessage = "ERROR (no return value) : " + formatExprForError(expr);
    hasError = true;
    return false;
}

// clean-environment
bool Evaluator::evalCleanEnv(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    // 檢查後方不可有參數
    if (expr->cdr && expr->cdr->type != SExpType::NIL) {
        errorMessage = "ERROR (incorrect number of arguments) : clean-environment";
        hasError = true;
        return false;
    }
    
    // 清起來 
    cleanEnvironment();
    
    result = createSystemMessage("environment cleaned");
    return true;
}

// exit(只是拿來檢查error用) 
bool Evaluator::evalExit(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    // 檢查後方不可有參數
    if (expr->cdr && expr->cdr->type != SExpType::NIL) {
        errorMessage = "ERROR (incorrect number of arguments) : exit";
        hasError = true;
        return false;
    }
    
    result = make_shared<SExp>(SExpType::NIL);
    return true;
}

// 檢查是否是列表
bool Evaluator::isList(shared_ptr<SExp> expr) {
    if (expr->type == SExpType::NIL) return true;
    if (expr->type != SExpType::PAIR) return false;
    
    shared_ptr<SExp> ptr = expr;
    while (ptr->type == SExpType::PAIR) {
        ptr = ptr->cdr;
    }
    
    return ptr->type == SExpType::NIL;
}

// 清起來 
void Evaluator::cleanEnvironment() {
    environment.clear();
    initPrimitives();
}

// get error message
string Evaluator::getErrorMessage() {
    return errorMessage;
}

// 檢查是否有error
bool Evaluator::hasEncounteredError() {
    return hasError;
}

// 重置error message狀態
void Evaluator::resetError() {
    hasError = false;
    errorMessage = "";
}

// 檢查參數數量用 
void Evaluator::checkArity(string name, vector<shared_ptr<SExp>> args, int expected, bool exact) {
    if ((exact && args.size() != expected) || (!exact && args.size() < expected)) {
        throw EvalError("incorrect number of arguments", name);
    }
}

// get num
double Evaluator::getNumber(shared_ptr<SExp> arg, string funcName) {
    if (arg->type == SExpType::INT) {
        return arg->intVal;
    } 
	else if (arg->type == SExpType::FLOAT) {
        return arg->floatVal;
    } 
	else {
        throw EvalError(funcName + " with incorrect argument type", "");
    }
}

// 比較兩個S-Exp是否相等（用於equal?）
bool Evaluator::isEqual(shared_ptr<SExp> a, shared_ptr<SExp> b) {
	// 如果兩個都是nullptr視為相等
    // 如果只有一個是nullptr則肯定不相等
    if (!a && !b) return true;
    if (!a || !b) return false;
    
    // 先檢查類型
    if (a->type != b->type) return false;
    
    // 再根據類型比較
    switch (a->type) {
        case SExpType::INT:
            return a->intVal == b->intVal;
        case SExpType::FLOAT:
            return a->floatVal == b->floatVal;
        case SExpType::STRING:
            return a->stringVal == b->stringVal;
        case SExpType::SYMBOL:
            return a->symbolVal == b->symbolVal;
        case SExpType::NIL:
        case SExpType::T:
            return true;
        case SExpType::PRIMITIVE_PROC:
            return a->procName == b->procName;
        case SExpType::PAIR:
            return isEqual(a->car, b->car) && isEqual(a->cdr, b->cdr);
        default:
            return false;
    }
}

// cons
shared_ptr<SExp> Evaluator::primitiveCons(vector<shared_ptr<SExp>>& args) {
    checkArity("cons", args, 2); // 確保參數數量
    return make_shared<SExp>(args[0], args[1]);
}

// list
shared_ptr<SExp> Evaluator::primitiveList(vector<shared_ptr<SExp>>& args) {
    shared_ptr<SExp> result = make_shared<SExp>(SExpType::NIL);
    
    // 從最後一個參數開始往前處理(為了構建正確順序的列表, 簡稱後入式)
    for (int i = args.size() - 1; i >= 0; i--) {
        result = make_shared<SExp>(args[i], result);
    }
    
    return result;
}

// car
shared_ptr<SExp> Evaluator::primitiveCar(vector<shared_ptr<SExp>>& args) {
    checkArity("car", args, 1); // 確保參數數量
    
    if (args[0]->type != SExpType::PAIR) {
        string errorValue;
        
        switch (args[0]->type) {
            case SExpType::INT:
                errorValue = to_string(args[0]->intVal);
                break;
            case SExpType::FLOAT: {
                ostringstream ss;
                double rounded = round(args[0]->floatVal * 1000) / 1000;
                ss << fixed << setprecision(3) << rounded;
                errorValue = ss.str();
                break;
            }
            case SExpType::STRING:
                errorValue = "\"" + args[0]->stringVal + "\"";
                break;
            case SExpType::SYMBOL:
                errorValue = args[0]->symbolVal;
                break;
            case SExpType::NIL:
                errorValue = "nil";
                break;
            case SExpType::T:
                errorValue = "#t";
                break;
            case SExpType::PRIMITIVE_PROC:
                errorValue = "#<procedure " + args[0]->procName + ">";
                break;
            case SExpType::USER_PROC:
                errorValue = "#<procedure " + args[0]->procName + ">";
                break;
            default:
                errorValue = formatExprForError(args[0]);
        }
        throw EvalError("car with incorrect argument type", errorValue);
    }
    
    return args[0]->car;
}

// cdr
shared_ptr<SExp> Evaluator::primitiveCdr(vector<shared_ptr<SExp>>& args) {
    checkArity("cdr", args, 1); // 確保參數數量 
    
    if (args[0]->type != SExpType::PAIR) {
        string errorValue;
        
        switch (args[0]->type) {
            case SExpType::INT:
                errorValue = to_string(args[0]->intVal);
                break;
            case SExpType::FLOAT: {
                ostringstream ss;
                double rounded = round(args[0]->floatVal * 1000) / 1000;
                ss << fixed << setprecision(3) << rounded;
                errorValue = ss.str();
                break;
            }
            case SExpType::STRING:
                errorValue = "\"" + args[0]->stringVal + "\"";
                break;
            case SExpType::SYMBOL:
                errorValue = args[0]->symbolVal;
                break;
            case SExpType::NIL:
                errorValue = "nil";
                break;
            case SExpType::T:
                errorValue = "#t";
                break;
            case SExpType::PRIMITIVE_PROC:
                errorValue = "#<procedure " + args[0]->procName + ">";
                break;
            case SExpType::USER_PROC:
                errorValue = "#<procedure " + args[0]->procName + ">";
                break;
            default:
                errorValue = "";
        }
        throw EvalError("cdr with incorrect argument type", errorValue);
    }
    
    return args[0]->cdr;
}

// atom嗎 
shared_ptr<SExp> Evaluator::primitiveAtom(vector<shared_ptr<SExp>>& args) {
    checkArity("atom?", args, 1); // 確保參數數量  
    
    bool isAtom = (args[0]->type != SExpType::PAIR);
    return isAtom ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// pair嗎 
shared_ptr<SExp> Evaluator::primitivePair(vector<shared_ptr<SExp>>& args) {
    checkArity("pair?", args, 1); // 確保參數數量  
    
    bool isPair = (args[0]->type == SExpType::PAIR);
    return isPair ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// list嗎 
shared_ptr<SExp> Evaluator::primitiveIsList(vector<shared_ptr<SExp>>& args) {
    checkArity("list?", args, 1); // 確保參數數量  
    
    if (args[0]->type == SExpType::NIL) {
        return make_shared<SExp>(SExpType::T);
    }
    
    if (args[0]->type != SExpType::PAIR) {
        return make_shared<SExp>(SExpType::NIL);
    }
    
    // 檢查是否為正確的列表結構
    shared_ptr<SExp> ptr = args[0];
    while (ptr->type == SExpType::PAIR) {
        ptr = ptr->cdr;
    }
    
    bool isList = (ptr->type == SExpType::NIL); // 縮一下程式碼 
    return isList ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// null嗎 
shared_ptr<SExp> Evaluator::primitiveNull(vector<shared_ptr<SExp>>& args) {
    checkArity("null?", args, 1); // 確保參數數量  
    
    bool isNull = (args[0]->type == SExpType::NIL);
    return isNull ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// integer?
shared_ptr<SExp> Evaluator::primitiveInteger(vector<shared_ptr<SExp>>& args) {
    checkArity("integer?", args, 1); // 確保參數數量  
    
    bool isInteger = (args[0]->type == SExpType::INT);
    return isInteger ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// real?
shared_ptr<SExp> Evaluator::primitiveReal(vector<shared_ptr<SExp>>& args) {
    checkArity("real?", args, 1); // 確保參數數量  
    
    // 注意 : INT和FLOAT都是實數
    bool isReal = (args[0]->type == SExpType::FLOAT || args[0]->type == SExpType::INT);
    return isReal ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// number?
shared_ptr<SExp> Evaluator::primitiveNumber(vector<shared_ptr<SExp>>& args) {
    checkArity("number?", args, 1); // 確保參數數量  
    
    bool isNumber = (args[0]->type == SExpType::INT || args[0]->type == SExpType::FLOAT);
    return isNumber ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// string?
shared_ptr<SExp> Evaluator::primitiveString(vector<shared_ptr<SExp>>& args) {
    checkArity("string?", args, 1); // 確保參數數量  
    
    bool isString = (args[0]->type == SExpType::STRING);
    return isString ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// boolean?
shared_ptr<SExp> Evaluator::primitiveBoolean(vector<shared_ptr<SExp>>& args) {
    checkArity("boolean?", args, 1); // 確保參數數量  
    
    bool isBoolean = (args[0]->type == SExpType::T || args[0]->type == SExpType::NIL);
    return isBoolean ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// symbol?
shared_ptr<SExp> Evaluator::primitiveSymbol(vector<shared_ptr<SExp>>& args) {
    checkArity("symbol?", args, 1); // 確保參數數量  
    
    bool isSymbol = (args[0]->type == SExpType::SYMBOL);
    return isSymbol ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// +
shared_ptr<SExp> Evaluator::primitiveAdd(vector<shared_ptr<SExp>>& args) {
	// 檢查參數數量
    if (args.size() < 2) {   
        throw EvalError("incorrect number of arguments", "+");
    }
    
    bool hasFloat = false;
    int intResult = 0;
    double floatResult = 0.0;
    
    // 注意浮點樹中途突然出現 
    for (auto arg : args) {
        if (arg->type == SExpType::FLOAT) {
            if (!hasFloat) {
                hasFloat = true;
                floatResult = intResult;
            }
            
            floatResult = floatResult + arg->floatVal;
        } 
		else if (arg->type == SExpType::INT) {
            if (hasFloat) {
                floatResult = floatResult + arg->intVal;
            } 
			else {
                intResult = intResult + arg->intVal;
            }
        } 
		else { // 都不是 
            // 提供錯誤參數
            string errorValue;
            if (arg->type == SExpType::STRING) errorValue = "\"" + arg->stringVal + "\"";
            else if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::T) errorValue = "#t";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else if (arg->type == SExpType::USER_PROC ) errorValue = "#<procedure " + arg->procName + ">";
            else errorValue = "原神啟動";
            
            throw EvalError("+ with incorrect argument type", errorValue);
        }
    }
    
    if (hasFloat) {
        return make_shared<SExp>(floatResult);
    } 
	else {
        return make_shared<SExp>(intResult);
    }
}

// -
shared_ptr<SExp> Evaluator::primitiveSubtract(vector<shared_ptr<SExp>>& args) {
    if (args.size() < 2) {
        throw EvalError("incorrect number of arguments", "-");
    }
    
    bool hasFloat = false;
    int intResult = 0;
    double floatResult = 0.0;
    
    // 注意浮點樹中途突然出現 
    // 處理第一個參數
    if (args[0]->type == SExpType::FLOAT) {
        hasFloat = true;
        floatResult = args[0]->floatVal;
    } 
	else if (args[0]->type == SExpType::INT) {
        intResult = args[0]->intVal;
    } 
	else {
        throw EvalError("- with incorrect argument type", formatExprForError(args[0]));
    }
    
    // 處理其他參數
    for (int i = 1; i < args.size(); i++) {
        if (args[i]->type == SExpType::FLOAT) {
            if (!hasFloat) {
                hasFloat = true;
                floatResult = intResult;
            }
            
            floatResult = floatResult - args[i]->floatVal;
        } 
		else if (args[i]->type == SExpType::INT) {
            if (hasFloat) {
                floatResult = floatResult - args[i]->intVal;
            } 
			else {
                intResult = intResult - args[i]->intVal;
            }
        } 
		else {
            throw EvalError("- with incorrect argument type", formatExprForError(args[i]));
        }
    }
    
    if (hasFloat) {
        return make_shared<SExp>(floatResult);
    } 
	else {
        return make_shared<SExp>(intResult);
    }
}

// *
shared_ptr<SExp> Evaluator::primitiveMultiply(vector<shared_ptr<SExp>>& args) {
    // 檢查參數數量
    if (args.empty()) {
        throw EvalError("incorrect number of arguments", "*");
    }
    
    bool hasFloat = false;
    int intResult = 1;
    double floatResult = 1.0;
    
    // 注意浮點樹中途突然出現 
    for (auto arg : args) {
        if (arg->type == SExpType::FLOAT) {
            if (!hasFloat) {
                hasFloat = true;
                floatResult = intResult;
            }
            
            floatResult = floatResult * arg->floatVal;
        } 
		else if (arg->type == SExpType::INT) {
            if (hasFloat) {
                floatResult = floatResult * arg->intVal;
            } 
			else {
                intResult = intResult * arg->intVal;
            }
        } 
		else {
            string errorValue;
            
            if (arg->type == SExpType::STRING) {
                errorValue = "\"" + arg->stringVal + "\"";
            } 
			else if (arg->type == SExpType::SYMBOL) {
                errorValue = arg->symbolVal;
            } 
			else if (arg->type == SExpType::NIL) {
                errorValue = "nil";
            } 
			else if (arg->type == SExpType::T) {
                errorValue = "#t";
            } 
			else if (arg->type == SExpType::PAIR) {
                errorValue = formatExprForError(arg);
            } 
			else {
                errorValue = "原神啟動";
            }
            
            throw EvalError("* with incorrect argument type", errorValue);
        }
    }
    
    if (hasFloat) {
        return make_shared<SExp>(floatResult);
    } 
	else {
        return make_shared<SExp>((intResult));
    }
}

// /
shared_ptr<SExp> Evaluator::primitiveDivide(vector<shared_ptr<SExp>>& args) {
	// 檢查參數數量
    if (args.empty()) {
        throw EvalError("incorrect number of arguments", "/");
    }
    
    bool hasFloat = false;
    int intResult = 0;
    double floatResult = 0.0;
    
    // 注意浮點樹中途突然出現
    // 處理第一個參數
    if (args[0]->type == SExpType::FLOAT) {
        hasFloat = true;
        floatResult = args[0]->floatVal;
    } 
	else if (args[0]->type == SExpType::INT) {
        intResult = args[0]->intVal;
        floatResult = args[0]->intVal;
    } 
	else {
        string errorValue;
        
        if (args[0]->type == SExpType::STRING) errorValue = "\"" + args[0]->stringVal + "\"";
        else if (args[0]->type == SExpType::SYMBOL) errorValue = args[0]->symbolVal;
        else if (args[0]->type == SExpType::NIL) errorValue = "nil";
        else if (args[0]->type == SExpType::T) errorValue = "#t";
        else errorValue = "原神啟動"; 
        
        throw EvalError("/ with incorrect argument type", errorValue);
    }
    
    // 如果只有一個參數
    if (args.size() == 1) {
        if ((hasFloat && floatResult == 0) || (!hasFloat && intResult == 0)) {
            throw EvalError("", "/", true);  // 除以零錯誤
        }
        
        if (hasFloat) {
            return make_shared<SExp>(1.0 / floatResult);
        } 
		else {
            // 整數情況特殊處理
            if (intResult == 1) return make_shared<SExp>(1);
            else if (intResult == -1) return make_shared<SExp>(-1);
            else return make_shared<SExp>(0);
        }
    }
    
    // 處理多個參數
    for (int i = 1; i < args.size(); i++) {
        if (args[i]->type == SExpType::FLOAT) {
            hasFloat = true;
            
            // 檢查除數是否為0
            if (args[i]->floatVal == 0) {
                throw EvalError("", "/", true);  // 除以零錯誤
            }
            
            if (i == 1 && !hasFloat) {
                // 第一次遇到浮點數
                floatResult = intResult;
            }
            
            floatResult = floatResult / args[i]->floatVal;
        } 
		else if (args[i]->type == SExpType::INT) {
            // 檢查除數是否為0
            if (args[i]->intVal == 0) {
                throw EvalError("", "/", true);  // 除以零錯誤
            }
            
            if (hasFloat) {
                // 浮點數運算
                floatResult = floatResult / args[i]->intVal;
            } 
			else {
                // 整數除法
                intResult = intResult / args[i]->intVal;
                floatResult = floatResult / args[i]->intVal; // 同步更新浮點數結果
            }
        } 
		else {
            string errorValue;
            
            if (args[i]->type == SExpType::STRING) errorValue = "\"" + args[i]->stringVal + "\"";
            else if (args[i]->type == SExpType::SYMBOL) errorValue = args[i]->symbolVal;
            else if (args[i]->type == SExpType::NIL) errorValue = "nil";
            else if (args[i]->type == SExpType::T) errorValue = "#t";
            else errorValue = "原神啟動"; 
            
            throw EvalError("/ with incorrect argument type", errorValue);
        }
    }
    
    if (hasFloat) {
        return make_shared<SExp>(floatResult);
    } 
	else {
        return make_shared<SExp>(intResult);
    }
}

// not
shared_ptr<SExp> Evaluator::primitiveNot(vector<shared_ptr<SExp>>& args) {
	// 檢查參數數量
    if (args.size() != 1) {
        throw EvalError("incorrect number of arguments", "not");
    }
    
    bool isNil = (args[0]->type == SExpType::NIL);
    return isNil ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// >
shared_ptr<SExp> Evaluator::primitiveGreater(vector<shared_ptr<SExp>>& args) {
	// 檢查參數數量
    if (args.size() < 2) {
        throw EvalError("incorrect number of arguments", ">");
    }
    
    // step1 : 檢查所有參數是否為數字類型
    for (auto arg : args) {
        if (arg->type != SExpType::INT && arg->type != SExpType::FLOAT) {
            string errorValue;
            
            if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::STRING) errorValue = "\"" + arg->stringVal + "\"";
            else if (arg->type == SExpType::PRIMITIVE_PROC) errorValue = "#<procedure " + arg->procName + ">";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else if (arg->type == SExpType::T) errorValue = "#t";
            else errorValue = "原神啟動";
            
            throw EvalError("> with incorrect argument type", errorValue);
        }
    }
    
    // step2 : 確定所有參數都是數字就進行比較
    for (int i = 0; i < args.size() - 1; i++) {
        double val1 = (args[i]->type == SExpType::INT) ? args[i]->intVal : args[i]->floatVal;
        double val2 = (args[i+1]->type == SExpType::INT) ? args[i+1]->intVal : args[i+1]->floatVal;
        
        if (!(val1 > val2)) {
            return make_shared<SExp>(SExpType::NIL);
        }
    }
    
    return make_shared<SExp>(SExpType::T);
}

// >=
shared_ptr<SExp> Evaluator::primitiveGreaterEqual(vector<shared_ptr<SExp>>& args) {
	// 檢查參數數量
    if (args.size() < 2) {
        throw EvalError("incorrect number of arguments", ">=");
    }
    
    // step1 : 檢查所有參數是否為數字類型
    for (auto arg : args) {
        if (arg->type != SExpType::INT && arg->type != SExpType::FLOAT) {
            string errorValue;
            
            if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::STRING) errorValue = "\"" + arg->stringVal + "\"";
            else if (arg->type == SExpType::PRIMITIVE_PROC) errorValue = "#<procedure " + arg->procName + ">";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else if (arg->type == SExpType::T) errorValue = "#t";
            else errorValue = "原神啟動";
            
            throw EvalError(">= with incorrect argument type", errorValue);
        }
    }
    
    // step2 : 確定所有參數都是數字就進行比較
    for (int i = 0; i < args.size() - 1; i++) {
        double val1 = (args[i]->type == SExpType::INT) ? args[i]->intVal : args[i]->floatVal;
        double val2 = (args[i+1]->type == SExpType::INT) ? args[i+1]->intVal : args[i+1]->floatVal;
        
        if (!(val1 >= val2)) {
            return make_shared<SExp>(SExpType::NIL);
        }
    }
    
    return make_shared<SExp>(SExpType::T);
}

// <
shared_ptr<SExp> Evaluator::primitiveLess(vector<shared_ptr<SExp>>& args) {
	// 檢查參數數量
    if (args.size() < 2) {
        throw EvalError("incorrect number of arguments", "<");
    }
    
    // step1 : 檢查所有參數是否為數字類型
    for (auto arg : args) {
        if (arg->type != SExpType::INT && arg->type != SExpType::FLOAT) {
            string errorValue;
            
            if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::STRING) errorValue = "\"" + arg->stringVal + "\"";
            else if (arg->type == SExpType::PRIMITIVE_PROC) errorValue = "#<procedure " + arg->procName + ">";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else if (arg->type == SExpType::T) errorValue = "#t";
            else errorValue = "原神啟動";
            
            throw EvalError("< with incorrect argument type", errorValue);
        }
    }
    
    // step2 : 確定所有參數都是數字就進行比較
    for (int i = 0; i < args.size() - 1; i++) {
        double val1 = (args[i]->type == SExpType::INT) ? args[i]->intVal : args[i]->floatVal;
        double val2 = (args[i+1]->type == SExpType::INT) ? args[i+1]->intVal : args[i+1]->floatVal;
        
        if (!(val1 < val2)) {
            return make_shared<SExp>(SExpType::NIL);
        }
    }
    
    return make_shared<SExp>(SExpType::T);
}

// <=
shared_ptr<SExp> Evaluator::primitiveLessEqual(vector<shared_ptr<SExp>>& args) {
	// 檢查參數數量
    if (args.size() < 2) {
        throw EvalError("incorrect number of arguments", "<=");
    }
    
    // step1 : 檢查所有參數是否為數字類型
    for (auto arg : args) {
        if (arg->type != SExpType::INT && arg->type != SExpType::FLOAT) {
            string errorValue;
            
            if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::STRING) errorValue = "\"" + arg->stringVal + "\"";
            else if (arg->type == SExpType::PRIMITIVE_PROC) errorValue = "#<procedure " + arg->procName + ">";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else if (arg->type == SExpType::T) errorValue = "#t";
            else errorValue = "原神啟動";
            
            throw EvalError("<= with incorrect argument type", errorValue);
        }
    }
    
    // step2 : 確定所有參數都是數字就進行比較
    for (int i = 0; i < args.size() - 1; i++) {
        double val1 = (args[i]->type == SExpType::INT) ? args[i]->intVal : args[i]->floatVal;
        double val2 = (args[i+1]->type == SExpType::INT) ? args[i+1]->intVal : args[i+1]->floatVal;
        
        if (!(val1 <= val2)) {
            return make_shared<SExp>(SExpType::NIL);
        }
    }
    
    return make_shared<SExp>(SExpType::T);
}

// =
shared_ptr<SExp> Evaluator::primitiveNumEqual(vector<shared_ptr<SExp>>& args) {
	// 檢查參數數量
    if (args.size() < 2) {
        throw EvalError("incorrect number of arguments", "=");
    }
    
    // step1 : 檢查所有參數是否為數字類型
    for (auto arg : args) {
        if (arg->type != SExpType::INT && arg->type != SExpType::FLOAT) {
            string errorValue;
            
            if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::STRING) errorValue = "\"" + arg->stringVal + "\"";
            else if (arg->type == SExpType::PRIMITIVE_PROC) errorValue = "#<procedure " + arg->procName + ">";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else if (arg->type == SExpType::T) errorValue = "#t";
            else errorValue = "原神啟動";
            
            throw EvalError("= with incorrect argument type", errorValue);
        }
    }
    
    // step2 : 確定所有參數都是數字就進行比較
    for (int i = 0; i < args.size() - 1; i++) {
        double val1 = (args[i]->type == SExpType::INT) ? args[i]->intVal : args[i]->floatVal;
        double val2 = (args[i+1]->type == SExpType::INT) ? args[i+1]->intVal : args[i+1]->floatVal;
        
        if (val1 != val2) {
            return make_shared<SExp>(SExpType::NIL);
        }
    }
    
    return make_shared<SExp>(SExpType::T);
}

// string-append
shared_ptr<SExp> Evaluator::primitiveStringAppend(vector<shared_ptr<SExp>>& args) {
    // 檢查參數數量
    if (args.size() < 2) { 
        throw EvalError("incorrect number of arguments", "string-append");
    }
    
    string result = "";
    
    for (auto arg : args) {
    	// step1 : 檢查參數是否為字串
        if (arg->type != SExpType::STRING) {
            string errorValue;
            
            if (arg->type == SExpType::INT) errorValue = to_string(arg->intVal);
            else if (arg->type == SExpType::FLOAT) errorValue = to_string(arg->floatVal);
            else if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::T) errorValue = "#t";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else errorValue = "原神啟動";
            
            throw EvalError("string-append with incorrect argument type", errorValue);
        }
        
        // step2 : 確定參數是字串就進行串聯 
        result = result + arg->stringVal;
    }
    
    return make_shared<SExp>(result, SExpType::STRING);
}

// string>?
shared_ptr<SExp> Evaluator::primitiveStringGreater(vector<shared_ptr<SExp>>& args) {
	// 檢查參數數量
    if (args.size() < 2) {
        throw EvalError("incorrect number of arguments", "string>?");
    }
    
    // step1 : 檢查所有參數是否為字串
    for (auto arg : args) {
        if (arg->type != SExpType::STRING) {
            string errorValue;
            
            if (arg->type == SExpType::INT) errorValue = to_string(arg->intVal);
            else if (arg->type == SExpType::FLOAT) errorValue = to_string(arg->floatVal);
            else if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::T) errorValue = "#t";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else errorValue = "原神啟動";
            
            throw EvalError("string>? with incorrect argument type", errorValue);
        }
    }
    
    // step2 : 確定所有參數都是字串就進行比較
    for (int i = 0; i < args.size() - 1; i++) {
        if (!(args[i]->stringVal > args[i+1]->stringVal)) {
            return make_shared<SExp>(SExpType::NIL);
        }
    }
    
    return make_shared<SExp>(SExpType::T);
}

// string<?
shared_ptr<SExp> Evaluator::primitiveStringLess(vector<shared_ptr<SExp>>& args) {
	// 檢查參數數量
    if (args.size() < 2) {
        throw EvalError("incorrect number of arguments", "string<?");
    }
    
    // step1 : 檢查所有參數是否為字串
    for (auto arg : args) {
        if (arg->type != SExpType::STRING) {
            string errorValue;
            
            if (arg->type == SExpType::INT) errorValue = to_string(arg->intVal);
            else if (arg->type == SExpType::FLOAT) errorValue = to_string(arg->floatVal);
            else if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::T) errorValue = "#t";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else errorValue = "原神啟動";
            
            throw EvalError("string<? with incorrect argument type", errorValue);
        }
    }
    
    // step2 : 確定所有參數都是字串就進行比較
    for (int i = 0; i < args.size() - 1; i++) {
        if (!(args[i]->stringVal < args[i+1]->stringVal)) {
            return make_shared<SExp>(SExpType::NIL);
        }
    }
    
    return make_shared<SExp>(SExpType::T);
}

// string=?
shared_ptr<SExp> Evaluator::primitiveStringEqual(vector<shared_ptr<SExp>>& args) {
	// 檢查參數數量
    if (args.size() < 2) {
        throw EvalError("incorrect number of arguments", "string=?");
    }
    
    // step1 : 檢查所有參數是否為字串
    for (auto arg : args) {
        if (arg->type != SExpType::STRING) {
            string errorValue;
            if (arg->type == SExpType::INT) errorValue = to_string(arg->intVal);
            else if (arg->type == SExpType::FLOAT) errorValue = to_string(arg->floatVal);
            else if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::T) errorValue = "#t";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else errorValue = "原神啟動";
            
            throw EvalError("string=? with incorrect argument type", errorValue);
        }
    }
    
    string str = args[0]->stringVal;
    
    // step2 : 確定所有參數都是字串就進行比較
    for (int i = 1; i < args.size(); i++) {
        if (args[i]->stringVal != str) {
            return make_shared<SExp>(SExpType::NIL);
        }
    }
    
    return make_shared<SExp>(SExpType::T);
}

// eqv?
shared_ptr<SExp> Evaluator::primitiveEqv(vector<shared_ptr<SExp>>& args) {
    checkArity("eqv?", args, 2); // 確保參數數量
    
    shared_ptr<SExp> a = args[0];
    shared_ptr<SExp> b = args[1];
    
    // step1 : 檢查是否為同一類型
    if (a->type != b->type) {
        return make_shared<SExp>(SExpType::NIL);
    }
    
    // step2 : 根據類型進行比較
    switch (a->type) {
        case SExpType::INT:
            return (a->intVal == b->intVal) ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
        case SExpType::FLOAT:
            return (a->floatVal == b->floatVal) ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
        case SExpType::STRING:
            // 對於字串應檢查是否是同一物件
            return (a.get() == b.get()) ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
        case SExpType::SYMBOL:
            return (a->symbolVal == b->symbolVal) ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
        case SExpType::NIL:
        case SExpType::T:
            return make_shared<SExp>(SExpType::T);
        case SExpType::PRIMITIVE_PROC:
            return (a->procName == b->procName) ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
        case SExpType::PAIR:
            // 對於點對只比較是否為同一物件
            return (a.get() == b.get()) ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
        default:
            return make_shared<SExp>(SExpType::NIL);
    }
}

// equal?
shared_ptr<SExp> Evaluator::primitiveEqual(vector<shared_ptr<SExp>>& args) {
    checkArity("equal?", args, 2); // 確保參數數量 
    
    return isEqual(args[0], args[1]) ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

shared_ptr<SExp> Evaluator::primitiveCreateErrorObject(vector<shared_ptr<SExp>>& args) {
    checkArity("create-error-object", args, 1);
    
    if (args[0]->type != SExpType::STRING) {
        throw EvalError("incorrect argument type", "create-error-object");
    }
    
    shared_ptr<SExp> result = make_shared<SExp>();
    result->type = SExpType::ERROR;
    result->stringVal = args[0]->stringVal;
    return result;
}

shared_ptr<SExp> Evaluator::primitiveErrorObject(vector<shared_ptr<SExp>>& args) {
    checkArity("error-object?", args, 1);
    return (args[0]->type == SExpType::ERROR) ? 
           make_shared<SExp>(SExpType::T) : 
           make_shared<SExp>(SExpType::NIL);
}

shared_ptr<SExp> Evaluator::primitiveRead(vector<shared_ptr<SExp>>& args) {
    checkArity("read", args, 0);
    
    // 用全域的輸入管理器而不是創新的
    if (!globalInput || !globalLexer) {
        shared_ptr<SExp> errorObj = make_shared<SExp>();
        errorObj->type = SExpType::ERROR;
        errorObj->stringVal = "ERROR : No input available";
        return errorObj;
    }
    
    try {
        // 直接用全域 lexer 和 input 創新的 parser
        Parser parser(*globalLexer, *globalInput);
        shared_ptr<SExp> expr = parser.parse();
        if (parser.hasEncounteredError()) {
            shared_ptr<SExp> errorObj = make_shared<SExp>();
            errorObj->type = SExpType::ERROR;
            errorObj->stringVal = parser.getErrorMessage();
            return errorObj;
        }
        
        return expr;
    }
    catch (ParseError e) {
        // 創錯誤物件
        shared_ptr<SExp> errorObj = make_shared<SExp>();
        errorObj->type = SExpType::ERROR;
        
        if (e.isNoClosingQuote) {
            errorObj->stringVal = "ERROR (no closing quote) : END-OF-LINE encountered at Line " + to_string(e.line) + " Column " + to_string(e.column);
        }
        else if (e.isEOF) {
            errorObj->stringVal = "ERROR : END-OF-FILE encountered when there should be more input";
        }
        else {
            errorObj->stringVal = "ERROR (unexpected character) : line " + to_string(e.line) + " column " + to_string(e.column) + " character '" + e.tokenValue + "'";
        }
        
        return errorObj;
    }
}

shared_ptr<SExp> Evaluator::primitiveWrite(vector<shared_ptr<SExp>>& args) {
    checkArity("write", args, 1);
    
    Printer printer;
    printer.print(args[0]);
    
    return args[0]; // 傳原始參數
}

shared_ptr<SExp> Evaluator::primitiveDisplayString(vector<shared_ptr<SExp>>& args) {
    checkArity("display-string", args, 1);
    
    if (args[0]->type != SExpType::STRING && args[0]->type != SExpType::ERROR) {
        throw EvalError("incorrect argument type", "display-string");
    }
    
    // 直接印字串內容 (不含引號) 
    cout << args[0]->stringVal;
    return args[0]; // 傳原始參數
}

shared_ptr<SExp> Evaluator::primitiveNewline(vector<shared_ptr<SExp>>& args) {
    checkArity("newline", args, 0);
    
    cout << endl;
    return make_shared<SExp>(SExpType::NIL); // return nil
}

shared_ptr<SExp> Evaluator::primitiveSymbolToString(vector<shared_ptr<SExp>>& args) {
    checkArity("symbol->string", args, 1);
    
    if (args[0]->type != SExpType::SYMBOL) {
        throw EvalError("incorrect argument type", "symbol->string");
    }
    
    shared_ptr<SExp> result = make_shared<SExp>();
    result->type = SExpType::STRING;
    result->stringVal = args[0]->symbolVal;
    return result;
}

shared_ptr<SExp> Evaluator::primitiveNumberToString(vector<shared_ptr<SExp>>& args) {
    checkArity("number->string", args, 1);
    
    if (args[0]->type != SExpType::INT && args[0]->type != SExpType::FLOAT) {
        throw EvalError("incorrect argument type", "number->string");
    }
    
    shared_ptr<SExp> result = make_shared<SExp>();
    result->type = SExpType::STRING;
    
    if (args[0]->type == SExpType::INT) {
        result->stringVal = to_string(args[0]->intVal);
    }
    else { // FLOAT
        stringstream ss;
        ss << fixed << setprecision(3) << args[0]->floatVal;
        result->stringVal = ss.str();
    }
    
    return result;
}

shared_ptr<SExp> Evaluator::primitiveEval(vector<shared_ptr<SExp>>& args, Evaluator& evaluator) {
    checkArity("eval", args, 1);
    
    shared_ptr<SExp> result;
    bool success = evaluator.EvalSExp(args[0], result);
    
    if (!success) {
        throw EvalError(evaluator.getErrorMessage(), "eval");
    }
    
    return result;
}

bool Evaluator::evalSet(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    // 語法: (set! <symbol> <value>)
    if (!expr->cdr || expr->cdr->type != SExpType::PAIR ||
        !expr->cdr->car || expr->cdr->car->type != SExpType::SYMBOL ||
        !expr->cdr->cdr || expr->cdr->cdr->type != SExpType::PAIR ||
        !expr->cdr->cdr->car || expr->cdr->cdr->cdr->type != SExpType::NIL) {
        errorMessage = "ERROR (SET! format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    string symbol = expr->cdr->car->symbolVal;
    
    // 先求值第二個參數
    shared_ptr<SExp> value;
    if (!EvalSExp(expr->cdr->cdr->car, value)) {
        return false;
    }
    
    environment[symbol] = value;
    
    result = value;
    return true;
}

// 防止無限遞回函數
bool Evaluator::willUseLambdaHelper(shared_ptr<SExp> expr, unordered_set<string>& visited) {
    // 空表達式不用 lambda
    if (!expr) return false;
    
    // ATOM不會直接包含 lambda（除非是引用）
    if (expr->type != SExpType::PAIR && expr->type != SExpType::SYMBOL) {
        return false;
    }
    
    // 檢查是否引用一個 lambda
    if (expr->type == SExpType::SYMBOL) {
        string symbolName = expr->symbolVal;
        
        // 防止循環引用
        if (visited.find(symbolName) != visited.end()) {
            return false;
        }
        
        // 符號加到已訪問集合
        visited.insert(symbolName);
        
        // 查環境中的值
        auto it = environment.find(symbolName);
        if (it != environment.end()) {
            // 如果值是user定義函數
            if (it->second->type == SExpType::USER_PROC) {
                // 直接檢查是否是 lambda
                if (it->second->procName == "lambda") {
                    return true;
                }
                
                // 檢查函數體
                if (it->second->body.size() > 0) {
                    for (auto bodyExpr : it->second->body) {
                        if (willUseLambdaHelper(bodyExpr, visited)) {
                            return true;
                        }
                    }
                }
            }
        }
        
        // 移除符號 (允許在其他地方使用) 
        visited.erase(symbolName);
        return false;
    }
    
    // 處理 lambda 表達式
    if (expr->car && expr->car->type == SExpType::SYMBOL && expr->car->symbolVal == "lambda") {
        return true;
    }
    
    // 避免過度分析內部結構
    if (expr->car && expr->car->type == SExpType::SYMBOL) {
        string op = expr->car->symbolVal;
        
        // 對於 define 僅檢查定義的值的部分
        if (op == "define" && expr->cdr && expr->cdr->type == SExpType::PAIR) {
            if (expr->cdr->car->type == SExpType::SYMBOL) {
                // 變數定義: (define var value)
                if (expr->cdr->cdr && expr->cdr->cdr->type == SExpType::PAIR) {
                    return willUseLambdaHelper(expr->cdr->cdr->car, visited);
                }
            }
            else if (expr->cdr->car->type == SExpType::PAIR) {
                // 函數定義: (define (func args...) body...)
                // 檢查函數體
                shared_ptr<SExp> body = expr->cdr->cdr;
                while (body && body->type == SExpType::PAIR) {
                    if (willUseLambdaHelper(body->car, visited)) {
                        return true;
                    }
                    
                    body = body->cdr;
                }
            }
            return false;
        }
        
        // 對於 let 檢查初始值和表達式
        if (op == "let" && expr->cdr && expr->cdr->type == SExpType::PAIR) {
            // 檢查綁定列表中的初始值
            shared_ptr<SExp> bindings = expr->cdr->car;
            while (bindings && bindings->type == SExpType::PAIR) {
                if (bindings->car && bindings->car->type == SExpType::PAIR && bindings->car->cdr && bindings->car->cdr->type == SExpType::PAIR) {
                    // 檢查初始值
                    if (willUseLambdaHelper(bindings->car->cdr->car, visited)) {
                        return true;
                    }
                }
                
                bindings = bindings->cdr;
            }
            
            // 檢查 let 主體
            shared_ptr<SExp> body = expr->cdr->cdr;
            while (body && body->type == SExpType::PAIR) {
                if (willUseLambdaHelper(body->car, visited)) {
                    return true;
                }
                
                body = body->cdr;
            }
            
            return false;
        }
    }
    
    // 處理一般的列表結構（call function）
    
    // 檢查函數部分
    if (willUseLambdaHelper(expr->car, visited)) {
        return true;
    }
    
    // 檢查所有參數
    shared_ptr<SExp> args = expr->cdr;
    while (args && args->type == SExpType::PAIR) {
        if (willUseLambdaHelper(args->car, visited)) {
            return true;
        }
        
        args = args->cdr;
    }
    
    return false;
}

// 警察臨檢 
bool Evaluator::willUseLambda(shared_ptr<SExp> expr) {
    unordered_set<string> visited;
    return willUseLambdaHelper(expr, visited);
}

// 檢查是否為 (exit)
bool isExitExpression(shared_ptr<SExp> expr) {
    if (!expr || expr->type != SExpType::PAIR) {
        return false;
    }
    
    // 檢查是否是exit符號
    if (!expr->car || expr->car->type != SExpType::SYMBOL || expr->car->symbolVal != "exit") {
        return false;
    }
    
    // 確保exit後面沒有參數
    return expr->cdr && expr->cdr->type == SExpType::NIL;
}

int main() {
    cout << "Welcome to OurScheme!" << endl << endl;
    
    InputManager input;
    Lexer lexer(input);
    Evaluator evaluator;
    evaluator.setGlobalInput(&input, &lexer);
    bool exitFound = false;
    
    // 跑到沒東西或提造早結束就停
    while (!exitFound && !input.isEOF()) {
        cout << "> ";
        hasEvalError = false;  // 每個表達式求值前重置標記
        isTopLevel = true;     // 每次新的頂層表達式評估時重置為 true
        
        // 為每個S-Exp創新的解譯器
        Parser parser(lexer, input);
        shared_ptr<SExp> expr = parser.parse();
        
        if (parser.hasEncounteredError()) {
            cout << parser.getErrorMessage() << endl;
            
            // 如果是EOF錯誤，則退出
            if (parser.getErrorMessage().find("END-OF-FILE encountered") != string::npos) {
                cout << "Thanks for using OurScheme!" << endl;
                break;
            }
            else{
            	cout << endl;
			}
        } 
        else if (expr) { // 通過文法檢查進入這
            // 檢查 (exit)
            if (isExitExpression(expr)) {
                cout << endl << "Thanks for using OurScheme!" << endl;
                exitFound = true;
                break;
            }
            
            bool usesLambda = evaluator.willUseLambda(expr);
            
            if (usesLambda) haslambda = true;
            else haslambda = false;

            // 對所有表達式進行求值
            shared_ptr<SExp> resultSExp;
            
            evaluator.resetError();
            if (evaluator.EvalSExp(expr, resultSExp)) {
                // 成功印結果
                Printer printer;
                printer.print(resultSExp);
                cout << endl << endl;
            }
            else {
                // 失敗印錯誤訊息
                cout << evaluator.getErrorMessage() << endl << endl;
            }
        }
        
        // 檢查是否到達EOF
        if (input.isEOF() && !exitFound) {
            cout << "> ERROR (no more input) : END-OF-FILE encountered" << endl;
            cout << "Thanks for using OurScheme!" ;
            break;
        }
    }
    
    return 0;
}
