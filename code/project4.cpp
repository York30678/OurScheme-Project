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

bool hasEvalError = false; // �Ω�����O�_�����~�o��
bool isTopLevel = true; // �ΨӴ��̳��h
bool haslambda = false; // �s���z�}�k 
bool verboseMode = true; // �ѤF��== 

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

    function<shared_ptr<SExp>(vector<shared_ptr<SExp>>&)> procValue; // �s��ڪ����
    string procName; // �s��ƦW(�Lerror��)

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
    
    const char* what() const noexcept override { // �ۭq�������O 
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
        
    const char* what() const noexcept override { // throw and catch, call e.what()(�i�o���~�T��)
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
    bool isFisttimeChange = false; // �Ω������eS-Exp�ѤU�����e�O�_������

    InputManager() : currLine(0), currPos(0), eofReached(false), hasUnreadToken(false) {
        // testnum = trash
        string testLine;
        if (getline(cin, testLine)) {
        } 
        else {
            eofReached = true;
        }
    }
    
    // �����e�r��
    char getCurrentChar() {
        if (currLine >= lines.size()) { // �Ĥ@���I�scurrLine�O0, line is empty, line = 0
            if (!eofReached) { // ��l�OFalse, �o�ɭ�currLine = 0, currPos = 0
                readMoreLines(); // Ū�@��
                if (currLine >= lines.size()) {
                    return '\0';  // �u��EOF
                }
            } 
            else {
                return '\0';  // EOF
            }
        }
        
        if (currPos >= lines[currLine].length()) {
            return '\n';  // ���
        }
        
        return lines[currLine][currPos]; // Ū����estring����currPos�Ӧr��
    }
    
    // �e�i�@�Ӧr��
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
            // ���ʨ�U�@��
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
            advance();  // ���L�����
        }
    }
    
    // �P�_�O�_�����j��
    bool isSeparator(char c) {
        return isspace(c) || c == '(' || c == ')' || c == '\'' || c == '"' || c == ';' || c == '\0';
    }
    
    // ���ʨ�U�@��
    void moveToNextLine() {
        if (currLine < lines.size()) {
            currPos = lines[currLine].length();
            advance();  // �o�N���ʨ�U�@��
        }
    }
    
    // �O�s�@�ӥ�Ū��token
    void ungetToken(Token token) {
        hasUnreadToken = true;
        unreadToken = token;
    }
    
    // �ˬd�O�_����Ū��token
    bool hasToken() {
        return hasUnreadToken;
    }
    
    // �����Ū��token
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
    
        // �s�ɷ�e��m
        int temp = currPos;
    
        // �ˬd�椤�O�_�u���ťթM����
        string& line = lines[0];
        int i = currPos;
    
        // ���L�ť�
        while (i < line.length() && isspace(line[i])) {
            i++;
        }
    
        // �ˬd�O�_�����ѩΦ��
        bool isTrash = (i >= line.length() || line[i] == ';');
    
        // �٭��m
        currPos = temp;
        return isTrash;
    }

    void trashtoread(){
        if (!lines.empty()) { // ���`�즹line�N�u���s�@��input
            lines.erase(lines.begin()); // clean up lines
        }
        
        readMoreLines();
    }
    
    // �����e�檺����
    int getCurrentLineLength() {
        if (currLine < lines.size()) {
            return lines[currLine].length();
        }
        return 0;
    }
};

// ���R��
class Lexer {
private:
    InputManager& input;
    
    // �P�_string�O�_�����ļƦr
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
    
    // �B�zstring������q
    string processEscapeSequences(string str) {
        string result;
        
        for (int i = 0; i < str.length(); i++) {
            if (str[i] == '\\' && i + 1 < str.length()) {
                char next = str[i + 1];
                if (next == 'n' || next == 't' || next == '\\' || next == '"') {
                    // �S����q�r��
                    switch (next) {
                        case 'n': result += '\n'; break;
                        case 't': result += '\t'; break;
                        case '\\': result += '\\'; break;
                        case '"': result += '"'; break;
                    }
                    
                    i++; // ���L��q�r���᪺�r��
                }
				else {
                    // �D�S����q(�O�d��l�ϱ׿�) 
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
    
    // ��U�@��token
    Token getNextToken() {
        // �ˬd�O�_���Q�wŪ����token
        if (input.hasToken()) {
            return input.getToken();
        }
        
        // step1�G���L�ťթM����
        while (true) {
            input.skipWhitespace();
            
            if (input.getCurrentChar() == ';') {
                input.skipComment();
            } 
            else {
                break;
            }
        }
        
        // �ˬd�O�_��FEOF
        if (input.isEOF()) {
            return Token(SExpType::EOF_TOKEN, "EOF", input.getLineNumber(), input.getColumnNumber());
        }
        
        // step2�G�P�Otoken
        // �O�Ĥ@�Ӧr������m��Ū�i�hc
        // �o�̺⪺�O�u����line�Mcolumn�Ӥ��O�blines������m
        int startLine = input.getLineNumber();  // return currLine + 1
        int startCol = input.getColumnNumber(); // return Pros + 1
        // �]��l�Ƴ��O0

        char c = input.getCurrentChar(); // Ū����e�r�� 
        
        // �S��r�B�z
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
            input.advance();  // ���L�}�Y�����޸�
            c = input.getCurrentChar();
            
            // ���ˬd�O���O�X�z�Ŧr�� 
            if (c == '"') {
                input.advance();  // ���L���������޸�
                return Token(SExpType::STRING, "Empty_String", startLine, startCol); // �X�k�Ŧr��
            }
            
            string rawValue;  // ���B�z��q����lstring
            bool isEscaping = false;  // ��e�O�_�B����q���A
            
            while (true) {
                c = input.getCurrentChar();
                
                if (c == '\0') {
                    // EOF���r�Ŧꥼ����
                    return Token(SExpType::STRING, "", input.getLineNumber(), input.getColumnNumber() );
                } 
				else if (c == '\n' && !isEscaping) {
                    // ������r�Ŧꥼ����
                    return Token(SExpType::STRING, "", input.getLineNumber(), input.getCurrentLineLength() + 1);
                }
                
                if (isEscaping) {
                    // �w�b��q���A(�L�פU�@�Ӧr�O���򳣪�������) 
                    rawValue += '\\';  // �O�d��q�аO
                    rawValue += c;
                    isEscaping = false;
                } 
				else if (c == '\\') {
                    // �i�J��q���A(���ߧY�[�r) 
                    isEscaping = true;
                }
				else if (c == '"' && !isEscaping) {
                    // �D��q�����޸����string����
                    input.advance();  // ���L���������޸�
                    return Token(SExpType::STRING, processEscapeSequences(rawValue), startLine, startCol);
                }
				else {
                    // ���q�r
                    rawValue += c;
                }
                
                input.advance();
            }
        } 
        else {
            // Ū���Ÿ��μƦr
            string value;
            
            while (!input.isSeparator(input.getCurrentChar())) {
                value = value + input.getCurrentChar();
                input.advance();
            }
            
            // ������o column index ��V�b��S-Exp�᪺�U�@�Ӧr����m 

            // �S��Ÿ��B�z
            if (value == "nil" || value == "#f" || value == "()") {
                return Token(SExpType::NIL, "nil", startLine, startCol);
            } 
            else if (value == "t" || value == "#t") {
                return Token(SExpType::T, "#t", startLine, startCol);
            } 
            else if (value == ".") {
                return Token(SExpType::DOT, ".", startLine, startCol);
            }
            
            // �P�_�O�_���Ʀr�� 
            if (isValidNumber(value)) {
                if (value.find('.') != string::npos) {
                    return Token(SExpType::FLOAT, value, startLine, startCol);
                } 
                else {
                    return Token(SExpType::INT, value, startLine, startCol);
                }
            }
            
            // �w�]��SYMBOL 
            return Token(SExpType::SYMBOL, value, startLine, startCol);
        }
    }
};

// �y�k���R��
class Parser {
private:
    Lexer& lexer;
    InputManager& input;
    Token currentToken;
    bool hasError;
    string errorMessage;
    
    // �ѪR�Ĥ@����k 
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
                
                if (currentToken.value == "Empty_String") currentToken.value.clear(); // �o�O�Ŧr�ꪺ���p, ��ۤv�񪺩Ծ��M�� 
                
                string value = currentToken.value;
                //currentToken = lexer.getNextToken();
                return make_shared<SExp>(value, SExpType::STRING);
            }
            case SExpType::SYMBOL: {
                string value = currentToken.value; // token���e�s�Jvalue
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
    
    // �ѪR�ĤG����k
    shared_ptr<SExp> parseList() {
        currentToken = lexer.getNextToken(); // ���A���U�@�Ӧr�� 
        
        // null case
        if (currentToken.type == SExpType::RIGHT_PAREN) {
            //currentToken = lexer.getNextToken();
            return make_shared<SExp>(SExpType::NIL);
        }
        
        // �ѪR�C��Ĥ@�Ӥ���
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
        
        // �ˬd�O�_���I���ܪk
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
            

            //currentToken = lexer.getNextToken(); // �k�A���U�@�Ӧr(�wŪ��) 
            return make_shared<SExp>(first, rest);
        } 
        else {
            // �зǦC��ѪR
            vector<shared_ptr<SExp>> elements;
            elements.push_back(first);
            
            while (currentToken.type != SExpType::RIGHT_PAREN) {
            	
                if (currentToken.type == SExpType::EOF_TOKEN) {
                    throw ParseError(input.getLineNumber(), input.getColumnNumber(), "EOF", "')'", false, true);
                }
                
                if (currentToken.type == SExpType::DOT) {
                    // �I���ܪk
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
                    
                    // �c���I�ﵲ�c
                    shared_ptr<SExp> result = last;
                    for (int i = elements.size() - 1; i >= 0; i--) {
                        result = make_shared<SExp>(elements[i], result);
                    }
                    
                    return result;
                }
                
                // �ѪR�U�@�Ӥ���
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
            
            // �k�A���U�@�Ӧr 
            // currentToken = lexer.getNextToken();
            
            // �c�ش��q�C��
            shared_ptr<SExp> result = make_shared<SExp>(SExpType::NIL);
            for (int i = elements.size() - 1; i >= 0; i--) {
                result = make_shared<SExp>(elements[i], result);
            }
            
            return result;
        }
    }
    
    // �ѪR�ĤT����k 
    shared_ptr<SExp> parseQuote() {
        currentToken = lexer.getNextToken(); // �޸��U�@�Ӧr 
        
        // �ˬd�O�_�J�� EOF
        if (currentToken.type == SExpType::EOF_TOKEN) {
            throw ParseError(input.getLineNumber(), input.getColumnNumber(), "EOF", "", false, true); // �аO�� isEOF=true �����~ 
        }
        
        // �ѪR�ޥΪ���F��
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
        
        // �c�� (quote <quoted>) ��F��
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
    
    // �ѪR�@�ӧ��㪺S-Exp
    shared_ptr<SExp> parse() {
        try {
            // �ˬd�O�_��EOF
            if (currentToken.type == SExpType::EOF_TOKEN) {
                throw ParseError(input.getLineNumber(), input.getColumnNumber(), "EOF", "", false, true);
            }
            
            // �ھڥثetoken������ѪR��k
            shared_ptr<SExp> result;
            if (currentToken.type == SExpType::LEFT_PAREN) { // ��k�ĤG��
                result = parseList();
            } 
            else if (currentToken.type == SExpType::QUOTE) { // ��k�ĤT��
                result = parseQuote();
            } 
            else { // ��k�Ĥ@��
                result = parseAtom();
            }           
            
            //cout << "Befor !!! Line : " << input.getLineNumber() << ", Column Index : " << input.getColumnNumber()-1 << endl;
            // ���B��line�����input�������, column����eS-Exp���U�@�Ӧr������
            input.cleanup(); // �sinput��vector�M�Ũ�ѥ��B�̧������e
            input.resetLine(); // ���]�渹 = 0
            input.resetColumn(); // ���]�C�� = 0
            // ���ˬd�Ѿl����e���٦��L������ whitespace + tab + ���Ѳզ������e
            if (input.trashornot()){ // true�N�O���U���n�����M�Ť��e�hŪ�U�@��i��(�]�Ӧ�|���p���)
                input.trashtoread(); // Ū�@��
            }
  
            // �wŪ�U�@��token
            currentToken = lexer.getNextToken();
            // �O�s��etoken�H�K�U���ϥ�
            input.ungetToken(currentToken);
            
            return result;
        } 
		catch (ParseError e) {
            hasError = true;
            
            if (e.isNoClosingQuote) {
                errorMessage = "ERROR (no closing quote) : END-OF-LINE encountered at Line " + to_string(e.line) + " Column " + to_string(e.column);
                input.cleanup();
                input.resetLine(); // ���]�渹 = 0
                input.resetColumn(); // ���]�C�� = 0
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
                input.resetLine(); // ���]�渹 = 0
                input.resetColumn(); // ���]�C�� = 0
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
    
    // �ˬd�O�_�w��FEOF
    bool isAtEOF() {
        return currentToken.type == SExpType::EOF_TOKEN;
    }
};

// S-Exp���L��
class Printer {
public:
    void print(shared_ptr<SExp> sexp) {
        if (!sexp) return;
        
        switch (sexp->type) {
            case SExpType::INT:
                cout << sexp->intVal;
                break;
            case SExpType::FLOAT: {
                //�F�|�ˤ��J�L���W�n��
                double rounded = round(sexp->floatVal * 1000) / 1000;
                cout << fixed << setprecision(3) << rounded;
                break;
            }
            case SExpType::STRING:
                cout << "\"" << sexp->stringVal << "\"";
                break;
            case SExpType::SYSTEM_MESSAGE: // �t�Τ���function�B�@�����T��  
                cout << sexp->stringVal;  
                break;
            case SExpType::SYMBOL:
                cout << sexp->symbolVal;
                break;
            case SExpType::PRIMITIVE_PROC: // �O�d�r 
                cout << "#<procedure " << sexp->procName << ">";
                break;
            case SExpType::USER_PROC:  // �O�d�r 
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
        
        cout << "( ";  // ���A���᭱��@�ӪŮ�
        
        // �����B�z�Ĥ@�Ӥ���, ������
        if (pair->car) {
            if (pair->car->type == SExpType::PAIR) {
                // �p�G�O�C��, ���j�B�z
                printPair(pair->car, indent + 2);
            } else {
                // ��LAtom�����L 
                printAtom(pair->car);
            }
        }
        
        // �B�z�ѤU����
        shared_ptr<SExp> rest = pair->cdr;
        while (rest && rest->type == SExpType::PAIR) {
            cout << endl;
            for (int i = 0; i < indent + 2; i++) cout << " ";
            
            if (rest->car) {
                if (rest->car->type == SExpType::PAIR) {
                    // �p�G�O�C��N���j�B�z
                    printPair(rest->car, indent + 2);
                } else {
                    // Atom�����L 
                    printAtom(rest->car);
                }
            }
            
            rest = rest->cdr;
        }
        
        // �B�z�I���ܪk�����p
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
        
        // �L�k�A��
        cout << endl;
        for (int i = 0; i < indent; i++) cout << " ";
        cout << ")";
    }
    
    // �LAtom��F��
    void printAtom(shared_ptr<SExp> atom) {
        if (!atom) return;
        
        switch (atom->type) {
            case SExpType::INT:
                cout << atom->intVal;
                break;
            case SExpType::FLOAT: {
            	//�F�|�ˤ��J�L���W�n�� 
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
            case SExpType::PRIMITIVE_PROC:  // �O�d�r 
                cout << "#<procedure " << atom->procName << ">";
                break;
            case SExpType::USER_PROC:  // �O�d�r 
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

// S-Exp���澹 
// !!!�n�g��function�Ӧh�F, �ҥH��w�q�g�bclass�̭�, ��ڥ\��Ԩ�~���g!!! 
class Evaluator {
private:
    // ���ҡ]�d��Ρ^
    unordered_map<string, shared_ptr<SExp>> environment;
    InputManager* globalInput;
    Lexer* globalLexer;
    
    // ���~�B�z
    bool hasError;
    string errorMessage;
    
    // S-Exp��r��]�Ω���~�T���^
    string exprToString(shared_ptr<SExp> expr);
    
    // �ˬd�O�_�����ح�l��ƦW
    bool isPrimitive(string name);
    
    // ��l�ƩҦ���l���
    void initPrimitives();
    
    // ����List
    bool evalList(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    
    // eval�\�� 
    bool evalQuote(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    bool evalDefine(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    bool evalLambda(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    bool evalLet(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    bool evalIf(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    bool evalCond(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    bool evalAnd(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    bool evalOr(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    bool evalBegin(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    
    // �B�~�ˬd 
    bool evalCleanEnv(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    bool evalExit(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    
    // �ˬd�O�_List 
    bool isList(shared_ptr<SExp> expr);
    
    // !!! �¨�Ʀ��p��ŧi��static�n�F !!! 
    // �]���S-Exp����O�_���c�۵� 
    static bool isEqual(shared_ptr<SExp> a, shared_ptr<SExp> b);
    
    // �H�Uprimitive���� 

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

    // �غc�\�� 
    static shared_ptr<SExp> primitiveCons(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveList(vector<shared_ptr<SExp>>& args);
    
    // ���ȥ\��
    static shared_ptr<SExp> primitiveCar(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveCdr(vector<shared_ptr<SExp>>& args);
    
    // �P�_�\��
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
    
    // �|�h�B��
    static shared_ptr<SExp> primitiveAdd(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveSubtract(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveMultiply(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveDivide(vector<shared_ptr<SExp>>& args);
    
    // �ˬd�\�� 
    static shared_ptr<SExp> primitiveNot(vector<shared_ptr<SExp>>& args);
    
    // ����\�� 
    static shared_ptr<SExp> primitiveGreater(vector<shared_ptr<SExp>>& args); // > 
    static shared_ptr<SExp> primitiveGreaterEqual(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveLess(vector<shared_ptr<SExp>>& args); // <
    static shared_ptr<SExp> primitiveLessEqual(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveNumEqual(vector<shared_ptr<SExp>>& args);
    
    // �r��ާ@�\�� 
    static shared_ptr<SExp> primitiveStringAppend(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveStringGreater(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveStringLess(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveStringEqual(vector<shared_ptr<SExp>>& args);
    
    // �۵�����
    static shared_ptr<SExp> primitiveEqv(vector<shared_ptr<SExp>>& args);
    static shared_ptr<SExp> primitiveEqual(vector<shared_ptr<SExp>>& args);
    
    // �ˬd�ѼƼƶq��
    static void checkArity(string name, vector<shared_ptr<SExp>> args, int expected, bool exact = true);
    
    // ���ȡ]�B�zINT�MFLOAT���ഫ�^
    static double getNumber(shared_ptr<SExp> arg, string funcName);
    
    // �u�n�A�ŧi�@��Printer�ӦLERROR����褺�e (�]���expr���e�S��k�榡�ƿ�X) 
    static string formatExprForError(shared_ptr<SExp> expr) {
        if (!expr) return "nil";
        
        // �ڬO�Ѥ~ 
        streambuf* oldCout = cout.rdbuf(); // �Ȧs�쥻cout����X�ؼС]streambuf�^
        stringstream ss; // �r���X�y 
        cout.rdbuf(ss.rdbuf()); // �Φ��k��cout����X�令�g�Jss
        
        // ��Printer��n�L���榡�˥X�Ө�ss 
        Printer printer;
        printer.print(expr);
        
        cout.rdbuf(oldCout); // �O�o��cout����X�ؼг]�^�쥻��oldCout, �ϥ��N�O��_�쪬 
        
        return ss.str(); // �o���N�o��a���榡�ƪ�expr�F 
    }

public:
    // �غc�l
    Evaluator();
    
    // �_�lfunction 
    bool EvalSExp(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    
    // �Ÿ��D��
    bool evalSymbol(shared_ptr<SExp> expr, shared_ptr<SExp>& result);
    
    // �M�z����(clean-environment)
    void cleanEnvironment();
    
    // ���~�B�z�� 
    string getErrorMessage();
    bool hasEncounteredError();
    void resetError();
    
    // �ˬd�O�_�|�Ψ� lambda
    bool willUseLambda(shared_ptr<SExp> expr);
    bool willUseLambdaHelper(shared_ptr<SExp> expr, unordered_set<string>& visited);
    // �[SYSTEM_MESSAGE
    shared_ptr<SExp> createSystemMessage(string message) {
        if (!verboseMode) {
            // �p�G�D verbose �Ҧ��N�^�� nil �Ӥ��OSYSMES 
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

// �غc�l 
Evaluator::Evaluator() : hasError(false) {
    initPrimitives();
}

// �n���Sԣ���� 
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
            
            // �B�z�Ĥ@�Ӥ���
            if (expr->car) {
                if (expr->car->type == SExpType::SYMBOL) {
                    ss << expr->car->symbolVal;
                } 
				else {
                    ss << exprToString(expr->car);
                }
            }
            
            // �B�z�ѤU����
            shared_ptr<SExp> rest = expr->cdr;
            while (rest && rest->type == SExpType::PAIR) {
                ss << " ";
                if (rest->car) {
                    ss << exprToString(rest->car);
                }
                
                rest = rest->cdr;
            }
            
            // �B�z�I���ܪk
            if (rest && rest->type != SExpType::NIL) {
                ss << " . " << exprToString(rest);
            }
            
            ss << ")";
            return ss.str();
        }
        default:
            return "�쯫�Ұ�";
    }
}

// �ˬd�O�_��Primitive Function�W
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

// ��l�ƩҦ�Primitive Function, �Q�M����|�A���[�J 
void Evaluator::initPrimitives() {
    // �غc�\�� 
    environment["cons"] = make_shared<SExp>("cons", primitiveCons);
    environment["list"] = make_shared<SExp>("list", primitiveList);
    
    // ���ȥ\��
    environment["car"] = make_shared<SExp>("car", primitiveCar);
    environment["cdr"] = make_shared<SExp>("cdr", primitiveCdr);
    
    // �P�_�\�� 
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
    
    // �|�h�B�� 
    environment["+"] = make_shared<SExp>("+", primitiveAdd);
    environment["-"] = make_shared<SExp>("-", primitiveSubtract);
    environment["*"] = make_shared<SExp>("*", primitiveMultiply);
    environment["/"] = make_shared<SExp>("/", primitiveDivide);
    
    // �ˬd�\�� 
    environment["not"] = make_shared<SExp>("not", primitiveNot);
    
    // ����\�� 
    environment[">"] = make_shared<SExp>(">", primitiveGreater);
    environment[">="] = make_shared<SExp>(">=", primitiveGreaterEqual);
    environment["<"] = make_shared<SExp>("<", primitiveLess);
    environment["<="] = make_shared<SExp>("<=", primitiveLessEqual);
    environment["="] = make_shared<SExp>("=", primitiveNumEqual);
    
    // �r��ާ@�\�� 
    environment["string-append"] = make_shared<SExp>("string-append", primitiveStringAppend);
    environment["string>?"] = make_shared<SExp>("string>?", primitiveStringGreater);
    environment["string<?"] = make_shared<SExp>("string<?", primitiveStringLess);
    environment["string=?"] = make_shared<SExp>("string=?", primitiveStringEqual);
    
    // �۵�����
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
    
        // �O�s��e�� top-level ���A
        bool wasTopLevel = isTopLevel;
        
        // eval �Ϊ���F�����Ӧbtop-level����
        isTopLevel = true;
        
        bool success = this->EvalSExp(args[0], result);
    
        // ��_ top-level
        isTopLevel = wasTopLevel;
    
        if (!success) {
            // �����^�ǿ��~�Ӥ��O��error (�קK���~�]��) 
            shared_ptr<SExp> errorObj = make_shared<SExp>();
            errorObj->type = SExpType::ERROR;
            errorObj->stringVal = this->getErrorMessage();
            return errorObj;
        }
        
        // �p�G���G�OSYSMES�ݥߧY�L 
        if (result && result->type == SExpType::SYSTEM_MESSAGE) {
            Printer printer;
            printer.print(result);
            cout << endl;
        }        
		
		return result;
    });
    
    // verbose (����) 
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

// �}�l
bool Evaluator::EvalSExp(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    if (!expr) {
        result = make_shared<SExp>(SExpType::NIL);
        return true;
    }
    
    // �s��elevel���A
    bool wasTopLevel = isTopLevel;
    bool res;
    bool currTopLevel;
    
    // ���Ftop-level�~�A��L���]���Dtop-level(��X�b���~level���檺�\��)
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
            // �ۨD�Ȫ�F��
            result = expr;
            isTopLevel = wasTopLevel;  // �٭�top-level���A
            return true;
            
        case SExpType::SYMBOL:
            // �d�Ÿ��j�w
            res = evalSymbol(expr, result);
            // �T�O�良�j�w�Ÿ������~�B�z�O���T��
            if (!res) {
                // �O�d��l�� "unbound symbol" ���~�T��
                isTopLevel = wasTopLevel;  // �٭�top-level���A
                return false;
            }
            isTopLevel = wasTopLevel;  // �٭�top-level���A
            return res;
            
            
        case SExpType::PAIR:
            // �i�J�Dtop-level
            currTopLevel = isTopLevel;
            isTopLevel = false;
            
            // �ˬd�O�_�O�S��Φ�
            if (expr->car && expr->car->type == SExpType::SYMBOL) {
                string op = expr->car->symbolVal;
                if (op == "define" || op == "clean-environment" || op == "exit") { 
                    if (!wasTopLevel) { // �X�{�b���~level 
                        string upperOp;
                        if (op == "define") upperOp = "DEFINE";
                        else if (op == "clean-environment") upperOp = "CLEAN-ENVIRONMENT";
                        else upperOp = "EXIT";
                        
                        errorMessage = "ERROR (level of " + upperOp + ")";
                        hasError = true;
                        isTopLevel = wasTopLevel;  // �٭�top-level���A
                        return false;
                    }
                }
            }
            
            // �����C���F��
            res = evalList(expr, result);
            //if (errorMessage.find("ERROR (no return piyan)") != string::npos) return true; 
            //cout << "�F�A�Q" << errorMessage << endl; 
            // �p�G�b���h�Υ��ѥB���O�S��Φ�, �h���~�T���ഫ�� "no return value"
            // �����ק�w�g�O�S�w���������~�T��
            if (!res && currTopLevel && 
                !(expr->car && expr->car->type == SExpType::SYMBOL && 
                  (expr->car->symbolVal == "define" || expr->car->symbolVal == "clean-environment" || expr->car->symbolVal == "exit"))) {
                //cout << "�F�A�Q" << errorMessage << endl; 
                // �O�d�S�w���������~
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
                    
                    // ��������ק�
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
                    // �N��L���~�ഫ�� "no return value"
                    errorMessage = "ERROR (no return value) : " + formatExprForError(expr) ;
                }
            }
            
            isTopLevel = currTopLevel;  // �٭�top-level���A
            return res;
            
        default:
            isTopLevel = wasTopLevel;  // �٭�top-level���A
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
    // step1 : �ˬdS-Exp�O�_�O���Ī�List 
    if (expr->cdr && expr->cdr->type != SExpType::NIL) {
        // �M��cdr�u, �T�O�̫�@��cdr�O NIL
        shared_ptr<SExp> current = expr->cdr;
        while (current->type == SExpType::PAIR) {
            current = current->cdr;
        }
        
        // �p�G�̫�@��cdr���O NIL, ���N��o�O�@���I�ﵲ�c, ���O����List 
        if (current->type != SExpType::NIL) {
            errorMessage = "ERROR (non-list) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
    }

    // step2 : S-Exp�ŦXList���c��A��function�ˬd
    if (!expr->car) {
        errorMessage = "ERROR (attempt to apply non-function) : " + formatExprForError(nullptr);
        hasError = true;
        return false;
    }
    
    // step3 : �ˬd�O�_�O�S��Φ�
    if (expr->car->type == SExpType::SYMBOL) {
        string symbol = expr->car->symbolVal;
        //cout << "�F�A�Q" << symbol << endl; 
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
    
    // step4 : �D��(��Ĥ@�Ӥ����i��D��)
    shared_ptr<SExp> op;
    string funcName = "";
    
    if (expr->car->type == SExpType::SYMBOL) {
        funcName = expr->car->symbolVal;
        
        // �ˬd���S���b���Ҹ� 
        auto it = environment.find(expr->car->symbolVal);
        if (it == environment.end()) { // �N��S�o�F�� 
            errorMessage = "ERROR (unbound symbol) : " + expr->car->symbolVal;
            hasError = true;
            return false;
        }
        
        op = it->second;
    } 
    else {
        // �����B��Ū�F��
        if (!EvalSExp(expr->car, op)) {
            errorMessage = "ERROR (no return piyan) : " + formatExprForError(expr->car);
            hasError = true;
            return false;
        }
        
        funcName = exprToString(expr->car);
    }
    
    // step5 : �T�{�D�ȫ᪺�Ĥ@�Ӥ����O�@�Ө��(��l��ƩΥΤ�w�q���)
    if (op->type != SExpType::PRIMITIVE_PROC && op->type != SExpType::USER_PROC) {
        errorMessage = "ERROR (attempt to apply non-function) : " + formatExprForError(op);
        hasError = true;
        return false;
    }
    
    // step6 : �ѼƼƶq�ˬd(�w��Ҧ��������)
    int argCount = 0;
    shared_ptr<SExp> argCheck = expr->cdr;
    
    while (argCheck && argCheck->type == SExpType::PAIR) {
        argCount++;
        argCheck = argCheck->cdr;
    }
    
    // �ˬd�Τ�w�q��ƪ��ѼƼƶq
    if (op->type == SExpType::USER_PROC) {
        if (argCount != op->params.size()) {
            errorMessage = "ERROR (incorrect number of arguments) : " + op->procName;
            hasError = true;
            return false;
        }
    }
    
    // �ˬd���P��l��ƹ������ѼƼƶq�O�_���T
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
    
    // step7 : ����ѼƨD�� 
    vector<shared_ptr<SExp>> args;
    shared_ptr<SExp> argList = expr->cdr;
    
    while (argList && argList->type != SExpType::NIL) {
        // �T�O�ѼƦC��ŦXPAIR���c
        if (argList->type != SExpType::PAIR) {
            errorMessage = "ERROR (non-list) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        shared_ptr<SExp> argVal;
        bool oldTopLevel = isTopLevel;
        isTopLevel = false; // �{�b���btop-level 
        
        if (!EvalSExp(argList->car, argVal)) { 
            // �ѼƨD�ȥ��Ѯɪ��B�z
            
            // ���ˬd�O�_�O���n�����~�����n�u���O�d
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
                // �O�d��������ק�
            }
            // �M���ˬd�O�_�O���j�w�Ÿ����~
            else if (errorMessage.find("ERROR (unbound symbol)") != string::npos) {
                // �O�d "unbound symbol" ���~
            } 
            // ���۳B�z��N�B���ƪ��ѼƨD�ȥ���
            else if (expr->car && expr->car->type == SExpType::SYMBOL && 
                (expr->car->symbolVal == "+" || expr->car->symbolVal == "-" || 
                 expr->car->symbolVal == "*" || expr->car->symbolVal == "/")) {
                // �O��N�B���ƪ��ѼƨD�ȥ��ѱN���~�ର "unbound parameter"
                errorMessage = "ERROR (unbound piyan) : " + formatExprForError(argList->car);
            }
            // �̫�B�z "no return value" ���~
            else if (errorMessage.find("ERROR (no return value)") != string::npos) {
                //  "no return value" ���~�ର "unbound parameter"
                errorMessage = "ERROR (unbound papapa) : " + formatExprForError(argList->car);
            }
            else {
                // ��L���p�ର "unbound parameter"
                errorMessage = "ERROR (unbound parameter) : " + formatExprForError(argList->car);
            }
            
            hasError = true;
            isTopLevel = oldTopLevel;
            return false;
        }
        
        isTopLevel = oldTopLevel;
        
        args.push_back(argVal); // ��C�ӰѼƨD�ȫ᪺���G�s�_�� 
        argList = argList->cdr; // �U���@�� 
    }
      // step8 : �S�������ܷ|���U�Ѽư��浲�G 
    try {
        // �B�z��l���
        if (op->type == SExpType::PRIMITIVE_PROC) {
            result = op->procValue(args);
            return true;
        }
        // �B�z�Τ�w�q���
        else if (op->type == SExpType::USER_PROC) {
            // �O�s��e����
            
            auto oldEnv = environment;
            
            // �зs���Үھڨ�������өw
            unordered_map<string, shared_ptr<SExp>> newEnv;
            
            if (op->env == nullptr) {
                // Lambda�G�η�e������
                
                newEnv = environment;
            }            
            else {
			
                // ���quser�w�q��ơG�Ψ�Ʃw�q�ɪ����ҧ@����¦
                newEnv = *(op->env);
                
                // �N��e���Ҥ��s�w�q����ƩM�ܼƦX�ֶi��
                // �o�˦b let �w�q�������ܼƩM�~�h�s�w�q����Ƴ���ݨ� 
                for (auto pair : environment) {
                    // �ˬd�O�_�O��Ʃw�q�ɤ��s�b���s����
                    if (op->env->find(pair.first) == op->env->end()) {
                        // �s�w�q����Ʃ��ܼƥ[�����Ҥ�
                        newEnv[pair.first] = pair.second;
                    }
                    // �p�G�O let �Ъ��{���ܼƤ]�n�л\�즳��
                    else if (pair.second.get() != op->env->at(pair.first).get()) {
                        // �s���j�w���ηs����
                        newEnv[pair.first] = pair.second;
                    }
                }
            }
            
            // �j�w�Ѽƨ�s����
            for (int i = 0; i < op->params.size(); i++) {
                newEnv[op->params[i]] = args[i];
            }
            
            // �ϥηs����
            environment = newEnv;
            
            // ��������]�� begin�^
            shared_ptr<SExp> currentResult;
            bool success = true;
              for (int i = 0; i < op->body.size(); i++) {
                bool isLastExpr = (i == op->body.size() - 1);
                if (EvalSExp(op->body[i], currentResult)) {
                    // ���榨�\�ˬd�O�_�OSYSMES 
                    if (currentResult && currentResult->type == SExpType::SYSTEM_MESSAGE) {
                        // SYSMES�ݦL�]���D�O�̫�@�Ӫ�F���^
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
              // �u��_��ưѼ� (�O�d set! �M define ���ĪG) 
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
    
    // �p�G��o�h��ܦ��޿���~
    return false;
}

// quote
bool Evaluator::evalQuote(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    // �ˬd�ѼƼƶq
    if (!expr->cdr || expr->cdr->type != SExpType::PAIR || !expr->cdr->car || expr->cdr->cdr->type != SExpType::NIL) {
        errorMessage = "ERROR (incorrect number of arguments) : quote";
        hasError = true;
        return false;
    }
    
    // ����return���P�_���Ѽ�
    result = expr->cdr->car;
    return true;
}

// define
bool Evaluator::evalDefine(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    // �ˬd�ѼƼƶq
    if (!expr->cdr || expr->cdr->type != SExpType::PAIR) {
        errorMessage = "ERROR (DEFINE format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    // �ˬd�w�q����
    if (expr->cdr->car->type == SExpType::SYMBOL) {
        // �y�k: (define �Ÿ� ��)
        
        // �ˬd�y�k
        if (!expr->cdr->cdr || expr->cdr->cdr->type != SExpType::PAIR || 
            !expr->cdr->cdr->car || expr->cdr->cdr->cdr->type != SExpType::NIL) {
            errorMessage = "ERROR (DEFINE format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
   
        string symbol = expr->cdr->car->symbolVal;
        
        // �ˬd�O�_���O�d�r
        if (isPrimitive(symbol)) {
            errorMessage = "ERROR (DEFINE format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        // �D��
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
        // �[������
        environment[symbol] = value;
        
        result = createSystemMessage(symbol + " defined");
        return true;
    }
    else if (expr->cdr->car->type == SExpType::PAIR) {
        // �y�k: (define (��ƦW �Ѽ�...) �����...)
        // �ˬd��ƦW�O�_�O�Ÿ�
        if (!expr->cdr->car->car || expr->cdr->car->car->type != SExpType::SYMBOL) {
            errorMessage = "ERROR (DEFINE format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        string funcName = expr->cdr->car->car->symbolVal;
        
        // �ˬd�O�_���O�d�r
        if (isPrimitive(funcName)) {
            errorMessage = "ERROR (DEFINE format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        // �����Ѽ�
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
        
        // �T�O�ѼƦC�����O nil
        if (paramsList && paramsList->type != SExpType::NIL) {
            errorMessage = "ERROR (DEFINE format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        // �ˬd�����
        if (!expr->cdr->cdr || expr->cdr->cdr->type == SExpType::NIL) {
            errorMessage = "ERROR (DEFINE format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        // ���������
        vector<shared_ptr<SExp>> body;
        shared_ptr<SExp> bodyList = expr->cdr->cdr;
        while (bodyList && bodyList->type == SExpType::PAIR) {
            body.push_back(bodyList->car);
            bodyList = bodyList->cdr;
        }
        
        // �T�O�����C�����O nil
        if (bodyList && bodyList->type != SExpType::NIL) {
            errorMessage = "ERROR (DEFINE format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        // �����Ұƥ�
        shared_ptr<unordered_map<string, shared_ptr<SExp>>> envCopy = make_shared<unordered_map<string, shared_ptr<SExp>>>(environment);
        
        // �Ш�ƹ�H
        shared_ptr<SExp> funcObj = make_shared<SExp>(params, body, envCopy);
        funcObj->type = SExpType::USER_PROC;
        funcObj->procName = funcName;
        
        // �[������
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
    // �y�k: (lambda (�ѼƦC��) �����...)
    
    // step1 : �ˬd�ѼƳ���
    if (!expr->cdr || expr->cdr->type != SExpType::PAIR) {
        errorMessage = "ERROR (LAMBDA format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    // step2 : �ˬd�ѼƦC��榡
    shared_ptr<SExp> paramsList = expr->cdr->car;
    if (paramsList->type != SExpType::PAIR && paramsList->type != SExpType::NIL) {
        errorMessage = "ERROR (LAMBDA format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    // step3 : �����ѼƦW
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
    
    // �T�O�ѼƦC�����O nil
    if (current && current->type != SExpType::NIL) {
        errorMessage = "ERROR (LAMBDA format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    // step4 : �ˬd�����
    shared_ptr<SExp> bodyList = expr->cdr->cdr;
    if (!bodyList || bodyList->type == SExpType::NIL) {
        errorMessage = "ERROR (LAMBDA format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    // step5 : ����������F��
    vector<shared_ptr<SExp>> body;
    current = bodyList;
    while (current && current->type == SExpType::PAIR) {
        body.push_back(current->car);
        current = current->cdr;
    }
    
    // �T�O�����C�����O nil
    if (current && current->type != SExpType::NIL) {
        errorMessage = "ERROR (LAMBDA format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    // step6 : ���b�w�q�ɮ������ҦӬO�� nullptr
    // Lambda �b�Q�ήɤ~�|�����ɪ�����
    shared_ptr<unordered_map<string, shared_ptr<SExp>>> envCopy = nullptr;
    
    // step7 : �� lambda ��ƹ�H
    result = make_shared<SExp>(params, body, envCopy);
    result->procName = "lambda"; // �Ω����
    
    return true;
}

bool Evaluator::evalLet(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    // �y�k: (let ((�ܶq1 ��1) (�ܶq2 ��2)...) ��F��...)
    
    // step1 : �ˬd���ܶq�j�w�M�ܤ֤@�Ӫ�F��
    if (!expr->cdr || expr->cdr->type != SExpType::PAIR || 
        !expr->cdr->cdr || expr->cdr->cdr->type == SExpType::NIL) {
        errorMessage = "ERROR (LET format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    // step2 : �j�w�C�����O PAIR or NIL
    shared_ptr<SExp> bindingsList = expr->cdr->car;
    if (bindingsList->type != SExpType::PAIR && bindingsList->type != SExpType::NIL) {
        errorMessage = "ERROR (LET format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    // step3 : ���ˬd�Ҧ��j�w���榡���D��
    shared_ptr<SExp> current = bindingsList;
    while (current && current->type == SExpType::PAIR) {
        // �C�Ӹj�w�����O PAIR ����
        if (current->car->type != SExpType::PAIR) {
            errorMessage = "ERROR (LET format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
          // �j�w���Ĥ@�Ӥ��������O�Ÿ�
        if (!current->car->car || current->car->car->type != SExpType::SYMBOL) {
            errorMessage = "ERROR (LET format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
          // �ˬd�ܼƦW�O�_�O���ب�ƦW�]�S���ˬd�^
        string varName = current->car->car->symbolVal;
        if (environment.find(varName) != environment.end() && environment[varName]->type == SExpType::PRIMITIVE_PROC) {
            // �p�G���ոj�w���ب�ƦW���i��榡���~
            // !!!�ݰ_�ӹ���ƽեή�!!!
			// �� ���F�� 
            if (current->car->cdr && current->car->cdr->type == SExpType::PAIR &&
                current->car->cdr->car && current->car->cdr->car->type == SExpType::SYMBOL) {
                errorMessage = "ERROR (LET format) : " + formatExprForError(expr);
                hasError = true;
                return false;
            }
        }
        
        // �j�w�������ȳ���
        if (!current->car->cdr || current->car->cdr->type != SExpType::PAIR) {
            errorMessage = "ERROR (LET format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        // �j�w���Ȥ��ᥲ���O NIL�]�T�O�C�Ӹj�w�u���ܶq�W�M�@�ӭȡ^
        if (current->car->cdr->cdr && current->car->cdr->cdr->type != SExpType::NIL) {
            errorMessage = "ERROR (LET format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        // �ˬd�O�_���ȡ]����u���ܶq�W�S���Ȫ����p�^
        if (!current->car->cdr->car) {
            errorMessage = "ERROR (LET format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        current = current->cdr;
    }
    
    // �T�O�j�w�C�����O nil
    if (current && current->type != SExpType::NIL) {
        errorMessage = "ERROR (LET format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }    
	
	// step4 : ���b�����Һ�Ҧ��j�w����
    vector<pair<string, shared_ptr<SExp>>> bindings;
    current = bindingsList;
    
    while (current && current->type == SExpType::PAIR) {
        // ���ܶq�W
        string varName = current->car->car->symbolVal;
        
        // �b�����Ҥ��D�ȩҦ��j�w����
        shared_ptr<SExp> value;
        bool oldTopLevel = isTopLevel;
        isTopLevel = false; // let �j�w�D�Ȥ��b���h
        
        if (!EvalSExp(current->car->cdr->car, value)) {
            isTopLevel = oldTopLevel;
            return false;
        }
        
        isTopLevel = oldTopLevel;
        
        // �O�s�j�w���٤��[�����Ҥ�
        bindings.push_back(make_pair(varName, value));
        
        current = current->cdr;
    }
    
    // step5 : �зs���ҥu�s�ϰ�j�w���ܼƦW
    auto oldEnv = environment;
    vector<string> localVars;
    
    // �N�Ҧ��p��n���j�w�[�����Ҥ��ðO���ϰ��ܼ�
    for (auto binding : bindings) {
        localVars.push_back(binding.first);
        environment[binding.first] = binding.second;
    }    
	
	// step6 : ���� let �D��
    shared_ptr<SExp> body = expr->cdr->cdr;
    shared_ptr<SExp> lastResult;
    bool success = true;
    
    while (body && body->type == SExpType::PAIR) {
        bool isLastExpr = (body->cdr == nullptr || body->cdr->type == SExpType::NIL);
        
        if (EvalSExp(body->car, lastResult)) {
            // ���\����
            if (isLastExpr) {
                result = lastResult;
            }        
		} 
		else {
            // ���楢�ѫ�_�ϰ��ܼƦ��O�d set! �M define �]�������ܼ�
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
	
	// �u��_ let �j�w���ϰ��ܼ� (�O�d��L�ܧ�) 
    for (string varName : localVars) {
        if (oldEnv.find(varName) != oldEnv.end()) {
            environment[varName] = oldEnv[varName];
        } 
		else {
            environment.erase(varName);
        }
    }
    
    /* ���Ϋ�_��L�ܼƭ�]�G
     1. define �Ъ�������O�d�b��������
     2. set! �]���ܼ����ӫO�d�b��������
     3. �u�� let �j�w���ϰ��ܼƻݳQ��_
    */ 
    
    return success;
}

// if
bool Evaluator::evalIf(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    // �ˬd�Ѽ�
    shared_ptr<SExp> args = expr->cdr;
    
    // step1 : �ˬd�ѼƼƶq�O�_�b���\�d��
    if (!args || args->type != SExpType::PAIR || !args->cdr || args->cdr->type != SExpType::PAIR) {
        errorMessage = "ERROR (incorrect number of arguments) : if";
        hasError = true;
        return false;
    }
    
    // step2 : �ˬd����M��覡�l��O�_�̦h�u���@��(��)else���l 
    shared_ptr<SExp> elseBranch = args->cdr->cdr;
    if (elseBranch && elseBranch->type == SExpType::PAIR) {
        // �ˬdelse���l����O�_�٦���h�Ѽ�
        if (elseBranch->cdr && elseBranch->cdr->type != SExpType::NIL) {
            errorMessage = "ERROR (incorrect number of arguments) : if";
            hasError = true;
            return false;
        }
    }
    
    // step3 : �P�_����u�� 
    shared_ptr<SExp> conditionExpr = args->car;
    shared_ptr<SExp> condition;
    
    if (!EvalSExp(conditionExpr, condition)) {
        return false;
    }
    
    // step4 : �ھڱ��󪺵��G�說�l�� 
    // �Dnil�]�D#f�^ = true (#t)
    if (condition->type != SExpType::NIL) { // ����(#t) 
        return EvalSExp(args->cdr->car, result); // �����l1 
    } 
	else { // ����(#f) 
        // ���ˬd�O�_��(��)else���l 
        if (!elseBranch || elseBranch->type == SExpType::NIL) {
            // �S��else���l�N����if�����G�Onil 
            errorMessage = "ERROR (no return value) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        return EvalSExp(elseBranch->car, result); // // �����l2(else����) 
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
    
    // �B�z�C�Ӥl�y
    while (clauses && clauses->type == SExpType::PAIR) {
        shared_ptr<SExp> clause = clauses->car;
        
        // �ˬd�������榡
        if (!clause || clause->type != SExpType::PAIR) {
            errorMessage = "ERROR (COND format) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        shared_ptr<SExp> testResult;
        
        // �B�zelse
        if (clause->car->type == SExpType::SYMBOL && clause->car->symbolVal == "else" && clauses->cdr->type == SExpType::NIL) {   
            testResult = make_shared<SExp>(SExpType::T);
        } 
        else {
            if (!EvalSExp(clause->car, testResult)) {
                return false;
            }
        }
        
        // ����l�y��]�� begin �޿�^
        if (testResult->type != SExpType::NIL) {
            shared_ptr<SExp> bodyExpr = clause->cdr;
            
            // �p�G�S���l�y��N�ˬd�榡
            if (!bodyExpr || bodyExpr->type == SExpType::NIL) {
                errorMessage = "ERROR (COND format) : " + formatExprForError(expr);
                hasError = true;
                return false;
            }
            
            // ����l�y��]�� begin �޿�^
            shared_ptr<SExp> currentResult;
            
            while (bodyExpr && bodyExpr->type == SExpType::PAIR) {
                // ���m���~���A
                resetError();
                
                if (EvalSExp(bodyExpr->car, currentResult)) {
                    // ���\����
                    result = currentResult;
                } 
				else {
                    // ���楢�ѴN�ˬd���~����
                    
                    // �ˬd�O�_�n�ߧY���_
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
                        // �ߧY���_
                        hasError = true;
                        return false;
                    }
                    
                    // �p�G�O "no return value" �N�ˬd�O�_���̫�@�Ӫ�F��
                    if (bodyExpr->cdr && bodyExpr->cdr->type != SExpType::NIL) {
                        // ���O�̫�@�Ӫ�F���N�~�����U�@��
                        resetError();
                    } 
					else {
                        // �O�̫�@�Ӫ�F���B�S����^�ȥN����� 
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
            
            // �p�G������o�̥N��Ҧ���F�������\����F
            return true;
        }
        
        clauses = clauses->cdr;
    }
    
    // �Ҧ��l�y������
    errorMessage = "ERROR (no return value) : " + formatExprForError(expr);
    hasError = true;
    return false;
}

// and
bool Evaluator::evalAnd(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    shared_ptr<SExp> args = expr->cdr; // ��and���Ѽ� 
    
    // �S���ѼƮɪ�^ #f(�o�̷|�z��)
    if (!args || args->type == SExpType::NIL) {
        errorMessage = "ERROR (incorrect number of arguments) : and";
        hasError = true;
        return false;
    }
    
    // �ˬd�C�ӰѼ�, �o�{#f�N�� 
    while (args && args->type != SExpType::NIL) {
        if (args->type != SExpType::PAIR) {
            errorMessage = "ERROR (non-list) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        if (!EvalSExp(args->car, result)) {
            // �p�G�Oif��F����������楢�ѫh���unbound condition���~
            if (args->car->type == SExpType::PAIR && args->car->car && 
                args->car->car->type == SExpType::SYMBOL && args->car->car->symbolVal == "if") {
                errorMessage = "ERROR (unbound condition) : " + formatExprForError(args->car);
                hasError = true;
            }
            return false;
        }
        
        // �ˬd���G 
        if (result->type == SExpType::NIL) {
            return true;
        }
        
        // ���U�@�ӰѼ�
        args = args->cdr;
    }
    
    // �Ҧ��ѼƬ�#t
    return true;
}

// or
bool Evaluator::evalOr(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    shared_ptr<SExp> args = expr->cdr;
    
    // �S���ѼƮɪ�^ #f(�o�̷|�z��)
    if (!args || args->type == SExpType::NIL) {
        errorMessage = "ERROR (incorrect number of arguments) : or";
        hasError = true;
        return false;
    }
    
    // �ˬd�C�ӰѼ�, �o�{#t�N�� 
    while (args && args->type != SExpType::NIL) {
        if (args->type != SExpType::PAIR) {
            errorMessage = "ERROR (non-list) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        if (!EvalSExp(args->car, result)) {
            return false;
        }
        
        // �ˬd���G
        if (result->type != SExpType::NIL) {
            return true;
        }
        
        // ���U�@�ӰѼ�
        args = args->cdr;
    }
    
    // �Ҧ��ѼƬ�#f
    result = make_shared<SExp>(SExpType::NIL);
    return true;
}

// begin
bool Evaluator::evalBegin(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    shared_ptr<SExp> args = expr->cdr;
    
    // �S���ѼƮɪ�^���~
    if (!args || args->type == SExpType::NIL) {
        errorMessage = "ERROR (incorrect number of arguments) : begin";
        hasError = true;
        return false;
    }
    
    shared_ptr<SExp> currentResult;
    
    // �̧ǰ���Ҧ���F��
    while (args && args->type != SExpType::NIL) {
        if (args->type != SExpType::PAIR) {
            errorMessage = "ERROR (non-list) : " + formatExprForError(expr);
            hasError = true;
            return false;
        }
        
        // �ˬd�O�_���̫�@�Ӫ�F��
        bool isLastExpr = (args->cdr == nullptr || args->cdr->type == SExpType::NIL);
        
        // ���m���~���A
        resetError();
        
        if (EvalSExp(args->car, currentResult)) {
            // ���\����
            if (isLastExpr) {
                // �p�G�O�̫�@�Ӫ�F���B���\  
                result = currentResult;
                return true;
            }
            // �p�G���O�̫�@�Ӫ�F���N�~�����U�@��
        } 
		else {
            // ���楢�ѡA�ˬd���~����
            //cout << "�F�A�Q" <<  errorMessage << endl; 
            // �ˬd�O�_�ݥߧY���_
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
                // �ߧY���_
                hasError = true;
                return false;
            }
            
            // �p�G�O "no return value" ���~
            if (isLastExpr) {
            	//cout << "�F�A�Q" << errorMessage << endl;
                // �p�G�O�̫�@�Ӫ�F�����ѥN����� 
                errorMessage = "ERROR (no return value) : " + formatExprForError(expr);
                hasError = true;
                return false;
            }
            // �p�G���O�̫�@�Ӫ�F���N���� "no return value" �~�����U�@�Ӫ�F��
            resetError();
        }
        
        // ���U�@�Ӫ�F��
        args = args->cdr;
    }
    
    // �p�G��F�o�̥N��S����F���Q����]�z�פW���|�o�͡^
    errorMessage = "ERROR (no return value) : " + formatExprForError(expr);
    hasError = true;
    return false;
}

// clean-environment
bool Evaluator::evalCleanEnv(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    // �ˬd��褣�i���Ѽ�
    if (expr->cdr && expr->cdr->type != SExpType::NIL) {
        errorMessage = "ERROR (incorrect number of arguments) : clean-environment";
        hasError = true;
        return false;
    }
    
    // �M�_�� 
    cleanEnvironment();
    
    result = createSystemMessage("environment cleaned");
    return true;
}

// exit(�u�O�����ˬderror��) 
bool Evaluator::evalExit(shared_ptr<SExp> expr, shared_ptr<SExp>& result) {
    // �ˬd��褣�i���Ѽ�
    if (expr->cdr && expr->cdr->type != SExpType::NIL) {
        errorMessage = "ERROR (incorrect number of arguments) : exit";
        hasError = true;
        return false;
    }
    
    result = make_shared<SExp>(SExpType::NIL);
    return true;
}

// �ˬd�O�_�O�C��
bool Evaluator::isList(shared_ptr<SExp> expr) {
    if (expr->type == SExpType::NIL) return true;
    if (expr->type != SExpType::PAIR) return false;
    
    shared_ptr<SExp> ptr = expr;
    while (ptr->type == SExpType::PAIR) {
        ptr = ptr->cdr;
    }
    
    return ptr->type == SExpType::NIL;
}

// �M�_�� 
void Evaluator::cleanEnvironment() {
    environment.clear();
    initPrimitives();
}

// get error message
string Evaluator::getErrorMessage() {
    return errorMessage;
}

// �ˬd�O�_��error
bool Evaluator::hasEncounteredError() {
    return hasError;
}

// ���merror message���A
void Evaluator::resetError() {
    hasError = false;
    errorMessage = "";
}

// �ˬd�ѼƼƶq�� 
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

// ������S-Exp�O�_�۵��]�Ω�equal?�^
bool Evaluator::isEqual(shared_ptr<SExp> a, shared_ptr<SExp> b) {
	// �p�G��ӳ��Onullptr�����۵�
    // �p�G�u���@�ӬOnullptr�h�֩w���۵�
    if (!a && !b) return true;
    if (!a || !b) return false;
    
    // ���ˬd����
    if (a->type != b->type) return false;
    
    // �A�ھ��������
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
    checkArity("cons", args, 2); // �T�O�ѼƼƶq
    return make_shared<SExp>(args[0], args[1]);
}

// list
shared_ptr<SExp> Evaluator::primitiveList(vector<shared_ptr<SExp>>& args) {
    shared_ptr<SExp> result = make_shared<SExp>(SExpType::NIL);
    
    // �q�̫�@�ӰѼƶ}�l���e�B�z(���F�c�إ��T���Ǫ��C��, ²�٫�J��)
    for (int i = args.size() - 1; i >= 0; i--) {
        result = make_shared<SExp>(args[i], result);
    }
    
    return result;
}

// car
shared_ptr<SExp> Evaluator::primitiveCar(vector<shared_ptr<SExp>>& args) {
    checkArity("car", args, 1); // �T�O�ѼƼƶq
    
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
    checkArity("cdr", args, 1); // �T�O�ѼƼƶq 
    
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

// atom�� 
shared_ptr<SExp> Evaluator::primitiveAtom(vector<shared_ptr<SExp>>& args) {
    checkArity("atom?", args, 1); // �T�O�ѼƼƶq  
    
    bool isAtom = (args[0]->type != SExpType::PAIR);
    return isAtom ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// pair�� 
shared_ptr<SExp> Evaluator::primitivePair(vector<shared_ptr<SExp>>& args) {
    checkArity("pair?", args, 1); // �T�O�ѼƼƶq  
    
    bool isPair = (args[0]->type == SExpType::PAIR);
    return isPair ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// list�� 
shared_ptr<SExp> Evaluator::primitiveIsList(vector<shared_ptr<SExp>>& args) {
    checkArity("list?", args, 1); // �T�O�ѼƼƶq  
    
    if (args[0]->type == SExpType::NIL) {
        return make_shared<SExp>(SExpType::T);
    }
    
    if (args[0]->type != SExpType::PAIR) {
        return make_shared<SExp>(SExpType::NIL);
    }
    
    // �ˬd�O�_�����T���C���c
    shared_ptr<SExp> ptr = args[0];
    while (ptr->type == SExpType::PAIR) {
        ptr = ptr->cdr;
    }
    
    bool isList = (ptr->type == SExpType::NIL); // �Y�@�U�{���X 
    return isList ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// null�� 
shared_ptr<SExp> Evaluator::primitiveNull(vector<shared_ptr<SExp>>& args) {
    checkArity("null?", args, 1); // �T�O�ѼƼƶq  
    
    bool isNull = (args[0]->type == SExpType::NIL);
    return isNull ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// integer?
shared_ptr<SExp> Evaluator::primitiveInteger(vector<shared_ptr<SExp>>& args) {
    checkArity("integer?", args, 1); // �T�O�ѼƼƶq  
    
    bool isInteger = (args[0]->type == SExpType::INT);
    return isInteger ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// real?
shared_ptr<SExp> Evaluator::primitiveReal(vector<shared_ptr<SExp>>& args) {
    checkArity("real?", args, 1); // �T�O�ѼƼƶq  
    
    // �`�N : INT�MFLOAT���O���
    bool isReal = (args[0]->type == SExpType::FLOAT || args[0]->type == SExpType::INT);
    return isReal ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// number?
shared_ptr<SExp> Evaluator::primitiveNumber(vector<shared_ptr<SExp>>& args) {
    checkArity("number?", args, 1); // �T�O�ѼƼƶq  
    
    bool isNumber = (args[0]->type == SExpType::INT || args[0]->type == SExpType::FLOAT);
    return isNumber ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// string?
shared_ptr<SExp> Evaluator::primitiveString(vector<shared_ptr<SExp>>& args) {
    checkArity("string?", args, 1); // �T�O�ѼƼƶq  
    
    bool isString = (args[0]->type == SExpType::STRING);
    return isString ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// boolean?
shared_ptr<SExp> Evaluator::primitiveBoolean(vector<shared_ptr<SExp>>& args) {
    checkArity("boolean?", args, 1); // �T�O�ѼƼƶq  
    
    bool isBoolean = (args[0]->type == SExpType::T || args[0]->type == SExpType::NIL);
    return isBoolean ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// symbol?
shared_ptr<SExp> Evaluator::primitiveSymbol(vector<shared_ptr<SExp>>& args) {
    checkArity("symbol?", args, 1); // �T�O�ѼƼƶq  
    
    bool isSymbol = (args[0]->type == SExpType::SYMBOL);
    return isSymbol ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// +
shared_ptr<SExp> Evaluator::primitiveAdd(vector<shared_ptr<SExp>>& args) {
	// �ˬd�ѼƼƶq
    if (args.size() < 2) {   
        throw EvalError("incorrect number of arguments", "+");
    }
    
    bool hasFloat = false;
    int intResult = 0;
    double floatResult = 0.0;
    
    // �`�N�B�I�𤤳~��M�X�{ 
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
		else { // �����O 
            // ���ѿ��~�Ѽ�
            string errorValue;
            if (arg->type == SExpType::STRING) errorValue = "\"" + arg->stringVal + "\"";
            else if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::T) errorValue = "#t";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else if (arg->type == SExpType::USER_PROC ) errorValue = "#<procedure " + arg->procName + ">";
            else errorValue = "�쯫�Ұ�";
            
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
    
    // �`�N�B�I�𤤳~��M�X�{ 
    // �B�z�Ĥ@�ӰѼ�
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
    
    // �B�z��L�Ѽ�
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
    // �ˬd�ѼƼƶq
    if (args.empty()) {
        throw EvalError("incorrect number of arguments", "*");
    }
    
    bool hasFloat = false;
    int intResult = 1;
    double floatResult = 1.0;
    
    // �`�N�B�I�𤤳~��M�X�{ 
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
                errorValue = "�쯫�Ұ�";
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
	// �ˬd�ѼƼƶq
    if (args.empty()) {
        throw EvalError("incorrect number of arguments", "/");
    }
    
    bool hasFloat = false;
    int intResult = 0;
    double floatResult = 0.0;
    
    // �`�N�B�I�𤤳~��M�X�{
    // �B�z�Ĥ@�ӰѼ�
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
        else errorValue = "�쯫�Ұ�"; 
        
        throw EvalError("/ with incorrect argument type", errorValue);
    }
    
    // �p�G�u���@�ӰѼ�
    if (args.size() == 1) {
        if ((hasFloat && floatResult == 0) || (!hasFloat && intResult == 0)) {
            throw EvalError("", "/", true);  // ���H�s���~
        }
        
        if (hasFloat) {
            return make_shared<SExp>(1.0 / floatResult);
        } 
		else {
            // ��Ʊ��p�S��B�z
            if (intResult == 1) return make_shared<SExp>(1);
            else if (intResult == -1) return make_shared<SExp>(-1);
            else return make_shared<SExp>(0);
        }
    }
    
    // �B�z�h�ӰѼ�
    for (int i = 1; i < args.size(); i++) {
        if (args[i]->type == SExpType::FLOAT) {
            hasFloat = true;
            
            // �ˬd���ƬO�_��0
            if (args[i]->floatVal == 0) {
                throw EvalError("", "/", true);  // ���H�s���~
            }
            
            if (i == 1 && !hasFloat) {
                // �Ĥ@���J��B�I��
                floatResult = intResult;
            }
            
            floatResult = floatResult / args[i]->floatVal;
        } 
		else if (args[i]->type == SExpType::INT) {
            // �ˬd���ƬO�_��0
            if (args[i]->intVal == 0) {
                throw EvalError("", "/", true);  // ���H�s���~
            }
            
            if (hasFloat) {
                // �B�I�ƹB��
                floatResult = floatResult / args[i]->intVal;
            } 
			else {
                // ��ư��k
                intResult = intResult / args[i]->intVal;
                floatResult = floatResult / args[i]->intVal; // �P�B��s�B�I�Ƶ��G
            }
        } 
		else {
            string errorValue;
            
            if (args[i]->type == SExpType::STRING) errorValue = "\"" + args[i]->stringVal + "\"";
            else if (args[i]->type == SExpType::SYMBOL) errorValue = args[i]->symbolVal;
            else if (args[i]->type == SExpType::NIL) errorValue = "nil";
            else if (args[i]->type == SExpType::T) errorValue = "#t";
            else errorValue = "�쯫�Ұ�"; 
            
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
	// �ˬd�ѼƼƶq
    if (args.size() != 1) {
        throw EvalError("incorrect number of arguments", "not");
    }
    
    bool isNil = (args[0]->type == SExpType::NIL);
    return isNil ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
}

// >
shared_ptr<SExp> Evaluator::primitiveGreater(vector<shared_ptr<SExp>>& args) {
	// �ˬd�ѼƼƶq
    if (args.size() < 2) {
        throw EvalError("incorrect number of arguments", ">");
    }
    
    // step1 : �ˬd�Ҧ��ѼƬO�_���Ʀr����
    for (auto arg : args) {
        if (arg->type != SExpType::INT && arg->type != SExpType::FLOAT) {
            string errorValue;
            
            if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::STRING) errorValue = "\"" + arg->stringVal + "\"";
            else if (arg->type == SExpType::PRIMITIVE_PROC) errorValue = "#<procedure " + arg->procName + ">";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else if (arg->type == SExpType::T) errorValue = "#t";
            else errorValue = "�쯫�Ұ�";
            
            throw EvalError("> with incorrect argument type", errorValue);
        }
    }
    
    // step2 : �T�w�Ҧ��ѼƳ��O�Ʀr�N�i����
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
	// �ˬd�ѼƼƶq
    if (args.size() < 2) {
        throw EvalError("incorrect number of arguments", ">=");
    }
    
    // step1 : �ˬd�Ҧ��ѼƬO�_���Ʀr����
    for (auto arg : args) {
        if (arg->type != SExpType::INT && arg->type != SExpType::FLOAT) {
            string errorValue;
            
            if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::STRING) errorValue = "\"" + arg->stringVal + "\"";
            else if (arg->type == SExpType::PRIMITIVE_PROC) errorValue = "#<procedure " + arg->procName + ">";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else if (arg->type == SExpType::T) errorValue = "#t";
            else errorValue = "�쯫�Ұ�";
            
            throw EvalError(">= with incorrect argument type", errorValue);
        }
    }
    
    // step2 : �T�w�Ҧ��ѼƳ��O�Ʀr�N�i����
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
	// �ˬd�ѼƼƶq
    if (args.size() < 2) {
        throw EvalError("incorrect number of arguments", "<");
    }
    
    // step1 : �ˬd�Ҧ��ѼƬO�_���Ʀr����
    for (auto arg : args) {
        if (arg->type != SExpType::INT && arg->type != SExpType::FLOAT) {
            string errorValue;
            
            if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::STRING) errorValue = "\"" + arg->stringVal + "\"";
            else if (arg->type == SExpType::PRIMITIVE_PROC) errorValue = "#<procedure " + arg->procName + ">";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else if (arg->type == SExpType::T) errorValue = "#t";
            else errorValue = "�쯫�Ұ�";
            
            throw EvalError("< with incorrect argument type", errorValue);
        }
    }
    
    // step2 : �T�w�Ҧ��ѼƳ��O�Ʀr�N�i����
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
	// �ˬd�ѼƼƶq
    if (args.size() < 2) {
        throw EvalError("incorrect number of arguments", "<=");
    }
    
    // step1 : �ˬd�Ҧ��ѼƬO�_���Ʀr����
    for (auto arg : args) {
        if (arg->type != SExpType::INT && arg->type != SExpType::FLOAT) {
            string errorValue;
            
            if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::STRING) errorValue = "\"" + arg->stringVal + "\"";
            else if (arg->type == SExpType::PRIMITIVE_PROC) errorValue = "#<procedure " + arg->procName + ">";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else if (arg->type == SExpType::T) errorValue = "#t";
            else errorValue = "�쯫�Ұ�";
            
            throw EvalError("<= with incorrect argument type", errorValue);
        }
    }
    
    // step2 : �T�w�Ҧ��ѼƳ��O�Ʀr�N�i����
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
	// �ˬd�ѼƼƶq
    if (args.size() < 2) {
        throw EvalError("incorrect number of arguments", "=");
    }
    
    // step1 : �ˬd�Ҧ��ѼƬO�_���Ʀr����
    for (auto arg : args) {
        if (arg->type != SExpType::INT && arg->type != SExpType::FLOAT) {
            string errorValue;
            
            if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::STRING) errorValue = "\"" + arg->stringVal + "\"";
            else if (arg->type == SExpType::PRIMITIVE_PROC) errorValue = "#<procedure " + arg->procName + ">";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else if (arg->type == SExpType::T) errorValue = "#t";
            else errorValue = "�쯫�Ұ�";
            
            throw EvalError("= with incorrect argument type", errorValue);
        }
    }
    
    // step2 : �T�w�Ҧ��ѼƳ��O�Ʀr�N�i����
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
    // �ˬd�ѼƼƶq
    if (args.size() < 2) { 
        throw EvalError("incorrect number of arguments", "string-append");
    }
    
    string result = "";
    
    for (auto arg : args) {
    	// step1 : �ˬd�ѼƬO�_���r��
        if (arg->type != SExpType::STRING) {
            string errorValue;
            
            if (arg->type == SExpType::INT) errorValue = to_string(arg->intVal);
            else if (arg->type == SExpType::FLOAT) errorValue = to_string(arg->floatVal);
            else if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::T) errorValue = "#t";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else errorValue = "�쯫�Ұ�";
            
            throw EvalError("string-append with incorrect argument type", errorValue);
        }
        
        // step2 : �T�w�ѼƬO�r��N�i����p 
        result = result + arg->stringVal;
    }
    
    return make_shared<SExp>(result, SExpType::STRING);
}

// string>?
shared_ptr<SExp> Evaluator::primitiveStringGreater(vector<shared_ptr<SExp>>& args) {
	// �ˬd�ѼƼƶq
    if (args.size() < 2) {
        throw EvalError("incorrect number of arguments", "string>?");
    }
    
    // step1 : �ˬd�Ҧ��ѼƬO�_���r��
    for (auto arg : args) {
        if (arg->type != SExpType::STRING) {
            string errorValue;
            
            if (arg->type == SExpType::INT) errorValue = to_string(arg->intVal);
            else if (arg->type == SExpType::FLOAT) errorValue = to_string(arg->floatVal);
            else if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::T) errorValue = "#t";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else errorValue = "�쯫�Ұ�";
            
            throw EvalError("string>? with incorrect argument type", errorValue);
        }
    }
    
    // step2 : �T�w�Ҧ��ѼƳ��O�r��N�i����
    for (int i = 0; i < args.size() - 1; i++) {
        if (!(args[i]->stringVal > args[i+1]->stringVal)) {
            return make_shared<SExp>(SExpType::NIL);
        }
    }
    
    return make_shared<SExp>(SExpType::T);
}

// string<?
shared_ptr<SExp> Evaluator::primitiveStringLess(vector<shared_ptr<SExp>>& args) {
	// �ˬd�ѼƼƶq
    if (args.size() < 2) {
        throw EvalError("incorrect number of arguments", "string<?");
    }
    
    // step1 : �ˬd�Ҧ��ѼƬO�_���r��
    for (auto arg : args) {
        if (arg->type != SExpType::STRING) {
            string errorValue;
            
            if (arg->type == SExpType::INT) errorValue = to_string(arg->intVal);
            else if (arg->type == SExpType::FLOAT) errorValue = to_string(arg->floatVal);
            else if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::T) errorValue = "#t";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else errorValue = "�쯫�Ұ�";
            
            throw EvalError("string<? with incorrect argument type", errorValue);
        }
    }
    
    // step2 : �T�w�Ҧ��ѼƳ��O�r��N�i����
    for (int i = 0; i < args.size() - 1; i++) {
        if (!(args[i]->stringVal < args[i+1]->stringVal)) {
            return make_shared<SExp>(SExpType::NIL);
        }
    }
    
    return make_shared<SExp>(SExpType::T);
}

// string=?
shared_ptr<SExp> Evaluator::primitiveStringEqual(vector<shared_ptr<SExp>>& args) {
	// �ˬd�ѼƼƶq
    if (args.size() < 2) {
        throw EvalError("incorrect number of arguments", "string=?");
    }
    
    // step1 : �ˬd�Ҧ��ѼƬO�_���r��
    for (auto arg : args) {
        if (arg->type != SExpType::STRING) {
            string errorValue;
            if (arg->type == SExpType::INT) errorValue = to_string(arg->intVal);
            else if (arg->type == SExpType::FLOAT) errorValue = to_string(arg->floatVal);
            else if (arg->type == SExpType::SYMBOL) errorValue = arg->symbolVal;
            else if (arg->type == SExpType::NIL) errorValue = "nil";
            else if (arg->type == SExpType::T) errorValue = "#t";
            else if (arg->type == SExpType::PAIR) errorValue = formatExprForError(arg);
            else errorValue = "�쯫�Ұ�";
            
            throw EvalError("string=? with incorrect argument type", errorValue);
        }
    }
    
    string str = args[0]->stringVal;
    
    // step2 : �T�w�Ҧ��ѼƳ��O�r��N�i����
    for (int i = 1; i < args.size(); i++) {
        if (args[i]->stringVal != str) {
            return make_shared<SExp>(SExpType::NIL);
        }
    }
    
    return make_shared<SExp>(SExpType::T);
}

// eqv?
shared_ptr<SExp> Evaluator::primitiveEqv(vector<shared_ptr<SExp>>& args) {
    checkArity("eqv?", args, 2); // �T�O�ѼƼƶq
    
    shared_ptr<SExp> a = args[0];
    shared_ptr<SExp> b = args[1];
    
    // step1 : �ˬd�O�_���P�@����
    if (a->type != b->type) {
        return make_shared<SExp>(SExpType::NIL);
    }
    
    // step2 : �ھ������i����
    switch (a->type) {
        case SExpType::INT:
            return (a->intVal == b->intVal) ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
        case SExpType::FLOAT:
            return (a->floatVal == b->floatVal) ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
        case SExpType::STRING:
            // ���r�����ˬd�O�_�O�P�@����
            return (a.get() == b.get()) ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
        case SExpType::SYMBOL:
            return (a->symbolVal == b->symbolVal) ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
        case SExpType::NIL:
        case SExpType::T:
            return make_shared<SExp>(SExpType::T);
        case SExpType::PRIMITIVE_PROC:
            return (a->procName == b->procName) ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
        case SExpType::PAIR:
            // ����I��u����O�_���P�@����
            return (a.get() == b.get()) ? make_shared<SExp>(SExpType::T) : make_shared<SExp>(SExpType::NIL);
        default:
            return make_shared<SExp>(SExpType::NIL);
    }
}

// equal?
shared_ptr<SExp> Evaluator::primitiveEqual(vector<shared_ptr<SExp>>& args) {
    checkArity("equal?", args, 2); // �T�O�ѼƼƶq
    
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
    
    // �Υ��쪺��J�޲z���Ӥ��O�зs��
    if (!globalInput || !globalLexer) {
        shared_ptr<SExp> errorObj = make_shared<SExp>();
        errorObj->type = SExpType::ERROR;
        errorObj->stringVal = "ERROR : No input available";
        return errorObj;
    }
    
    try {
        // �����Υ��� lexer �M input �зs�� parser
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
        // �п��~����
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
    
    return args[0]; // �ǭ�l�Ѽ�
}

shared_ptr<SExp> Evaluator::primitiveDisplayString(vector<shared_ptr<SExp>>& args) {
    checkArity("display-string", args, 1);
    
    if (args[0]->type != SExpType::STRING && args[0]->type != SExpType::ERROR) {
        throw EvalError("incorrect argument type", "display-string");
    }
    
    // �����L�r�ꤺ�e (���t�޸�) 
    cout << args[0]->stringVal;
    return args[0]; // �ǭ�l�Ѽ�
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
    // �y�k: (set! <symbol> <value>)
    if (!expr->cdr || expr->cdr->type != SExpType::PAIR ||
        !expr->cdr->car || expr->cdr->car->type != SExpType::SYMBOL ||
        !expr->cdr->cdr || expr->cdr->cdr->type != SExpType::PAIR ||
        !expr->cdr->cdr->car || expr->cdr->cdr->cdr->type != SExpType::NIL) {
        errorMessage = "ERROR (SET! format) : " + formatExprForError(expr);
        hasError = true;
        return false;
    }
    
    string symbol = expr->cdr->car->symbolVal;
    
    // ���D�ȲĤG�ӰѼ�
    shared_ptr<SExp> value;
    if (!EvalSExp(expr->cdr->cdr->car, value)) {
        return false;
    }
    
    environment[symbol] = value;
    
    result = value;
    return true;
}

// ����L�����^���
bool Evaluator::willUseLambdaHelper(shared_ptr<SExp> expr, unordered_set<string>& visited) {
    // �Ū�F������ lambda
    if (!expr) return false;
    
    // ATOM���|�����]�t lambda�]���D�O�ޥΡ^
    if (expr->type != SExpType::PAIR && expr->type != SExpType::SYMBOL) {
        return false;
    }
    
    // �ˬd�O�_�ޥΤ@�� lambda
    if (expr->type == SExpType::SYMBOL) {
        string symbolName = expr->symbolVal;
        
        // ����`���ޥ�
        if (visited.find(symbolName) != visited.end()) {
            return false;
        }
        
        // �Ÿ��[��w�X�ݶ��X
        visited.insert(symbolName);
        
        // �d���Ҥ�����
        auto it = environment.find(symbolName);
        if (it != environment.end()) {
            // �p�G�ȬOuser�w�q���
            if (it->second->type == SExpType::USER_PROC) {
                // �����ˬd�O�_�O lambda
                if (it->second->procName == "lambda") {
                    return true;
                }
                
                // �ˬd�����
                if (it->second->body.size() > 0) {
                    for (auto bodyExpr : it->second->body) {
                        if (willUseLambdaHelper(bodyExpr, visited)) {
                            return true;
                        }
                    }
                }
            }
        }
        
        // �����Ÿ� (���\�b��L�a��ϥ�) 
        visited.erase(symbolName);
        return false;
    }
    
    // �B�z lambda ��F��
    if (expr->car && expr->car->type == SExpType::SYMBOL && expr->car->symbolVal == "lambda") {
        return true;
    }
    
    // �קK�L�פ��R�������c
    if (expr->car && expr->car->type == SExpType::SYMBOL) {
        string op = expr->car->symbolVal;
        
        // ��� define ���ˬd�w�q���Ȫ�����
        if (op == "define" && expr->cdr && expr->cdr->type == SExpType::PAIR) {
            if (expr->cdr->car->type == SExpType::SYMBOL) {
                // �ܼƩw�q: (define var value)
                if (expr->cdr->cdr && expr->cdr->cdr->type == SExpType::PAIR) {
                    return willUseLambdaHelper(expr->cdr->cdr->car, visited);
                }
            }
            else if (expr->cdr->car->type == SExpType::PAIR) {
                // ��Ʃw�q: (define (func args...) body...)
                // �ˬd�����
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
        
        // ��� let �ˬd��l�ȩM��F��
        if (op == "let" && expr->cdr && expr->cdr->type == SExpType::PAIR) {
            // �ˬd�j�w�C������l��
            shared_ptr<SExp> bindings = expr->cdr->car;
            while (bindings && bindings->type == SExpType::PAIR) {
                if (bindings->car && bindings->car->type == SExpType::PAIR && bindings->car->cdr && bindings->car->cdr->type == SExpType::PAIR) {
                    // �ˬd��l��
                    if (willUseLambdaHelper(bindings->car->cdr->car, visited)) {
                        return true;
                    }
                }
                
                bindings = bindings->cdr;
            }
            
            // �ˬd let �D��
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
    
    // �B�z�@�몺�C���c�]call function�^
    
    // �ˬd��Ƴ���
    if (willUseLambdaHelper(expr->car, visited)) {
        return true;
    }
    
    // �ˬd�Ҧ��Ѽ�
    shared_ptr<SExp> args = expr->cdr;
    while (args && args->type == SExpType::PAIR) {
        if (willUseLambdaHelper(args->car, visited)) {
            return true;
        }
        
        args = args->cdr;
    }
    
    return false;
}

// ĵ���{�� 
bool Evaluator::willUseLambda(shared_ptr<SExp> expr) {
    unordered_set<string> visited;
    return willUseLambdaHelper(expr, visited);
}

// �ˬd�O�_�� (exit)
bool isExitExpression(shared_ptr<SExp> expr) {
    if (!expr || expr->type != SExpType::PAIR) {
        return false;
    }
    
    // �ˬd�O�_�Oexit�Ÿ�
    if (!expr->car || expr->car->type != SExpType::SYMBOL || expr->car->symbolVal != "exit") {
        return false;
    }
    
    // �T�Oexit�᭱�S���Ѽ�
    return expr->cdr && expr->cdr->type == SExpType::NIL;
}

int main() {
    cout << "Welcome to OurScheme!" << endl << endl;
    
    InputManager input;
    Lexer lexer(input);
    Evaluator evaluator;
    evaluator.setGlobalInput(&input, &lexer);
    bool exitFound = false;
    
    // �]��S�F��δ��y�������N��
    while (!exitFound && !input.isEOF()) {
        cout << "> ";
        hasEvalError = false;  // �C�Ӫ�F���D�ȫe���m�аO
        isTopLevel = true;     // �C���s�����h��F�������ɭ��m�� true
        
        // ���C��S-Exp�зs����Ķ��
        Parser parser(lexer, input);
        shared_ptr<SExp> expr = parser.parse();
        
        if (parser.hasEncounteredError()) {
            cout << parser.getErrorMessage() << endl;
            
            // �p�G�OEOF���~�A�h�h�X
            if (parser.getErrorMessage().find("END-OF-FILE encountered") != string::npos) {
                cout << "Thanks for using OurScheme!" << endl;
                break;
            }
            else{
            	cout << endl;
			}
        } 
        else if (expr) { // �q�L��k�ˬd�i�J�o
            // �ˬd (exit)
            if (isExitExpression(expr)) {
                cout << endl << "Thanks for using OurScheme!" << endl;
                exitFound = true;
                break;
            }
            
            bool usesLambda = evaluator.willUseLambda(expr);
            
            if (usesLambda) haslambda = true;
            else haslambda = false;

            // ��Ҧ���F���i��D��
            shared_ptr<SExp> resultSExp;
            
            evaluator.resetError();
            if (evaluator.EvalSExp(expr, resultSExp)) {
                // ���\�L���G
                Printer printer;
                printer.print(resultSExp);
                cout << endl << endl;
            }
            else {
                // ���ѦL���~�T��
                cout << evaluator.getErrorMessage() << endl << endl;
            }
        }
        
        // �ˬd�O�_��FEOF
        if (input.isEOF() && !exitFound) {
            cout << "> ERROR (no more input) : END-OF-FILE encountered" << endl;
            cout << "Thanks for using OurScheme!" ;
            break;
        }
    }
    
    return 0;
}
