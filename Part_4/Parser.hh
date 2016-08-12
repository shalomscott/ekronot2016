<?hh //decl

// Parses Jack into XML

/*
This main is in charge of looping through all of the .jack files in a source
directory and writing the parsed output into respective .xml files.
*/
function main()
{
    echo "This script takes Jack source and outputs the XML parse.\n";
    $srcDir = readline('Enter the source directory:  ');
    $srcDir = getcwd() . '/' . trim($srcDir); // getcwd == 'get current working directory'
    $paths = scandir($srcDir);
    array_splice($paths, 0, 2); // the first two elements in the array are: '.', '..' - these are unneeded
    foreach($paths as $key) {
        if (pathinfo($key, PATHINFO_EXTENSION) !== 'jack') // checks if the file has extension other than 'jack'. If so, skip over
            continue;
        echo "\nParsing " . $key . "...\n";
        $contents = file_get_contents("$srcDir/$key");
        try {
            // We start the recursive decent from the starting variable 'class'
            file_put_contents("$srcDir/" . strtok($key, '.') . 'S.xml', parseClass($contents));
        }
        catch(Exception $e) {
            echo $e->getMessage() , "\n";
        }
    }
}

// Regular expressions defining language constructs. The order of expressions in this array is important.
$regExps = [
    'integerConstant' => '/\d+/',
    'stringConstant' => '/"[^"\n]*"/',
    'symbol' => '/{|}|\(|\)|\[|\]|\.|,|;|,|\+|-|\*|\/|&|\||<|>|=|~/',
    'keyword' => '/class|constructor|function|method|field|static|var|int|char|boolean|void|true|false|null|this|let|do|if|else|while|return/',
    'identifier' => '/[a-zA-Z_]+\w*/'
];

function parseErr(string $str, string $errMsg) : void
{
    throw new Exception('Parse Error:  ' . $errMsg . "\nString remaining:\n" . $str);
}

/*
Description:
This function returns the next token to be processed out of the input.

Parameters: (All parameters are passed by-reference)
in str = The remaining code yet to be parsed. This function changes the original string object.
out type = The type of the token parsed is placed in this variable.
out value = The value of the token parsed is placed in this variable.
*/
function getNextTok(string &$str, ?string &$type, ?string &$value) : bool
{
    $str = ltrim($str);
    if (strlen($str) === 0) return false;

    if (substr($str, 0, 2) === '//') {
        if (($pos = strpos($str, "\n")) !== false) {
            $str = substr($str, $pos + 1);
            return getNextTok($str, $type, $value);
        }
        else {
            return false;
        }
    }

    if (substr($str, 0, 3) === '/**') {
        if (($pos = strpos($str, '*/')) !== false) {
            $str = substr($str, $pos + 2);
            return getNextTok($str, $type, $value);
        }
        else {
            return false;
        }
    }

    foreach($GLOBALS['regExps'] as $key => $regex) {
        if (preg_match($regex, $str, $match, PREG_OFFSET_CAPTURE) && $match[0][1] === 0) {
            $type = $key;
            $value = $match[0][0];
            $str = substr($str, strlen($value));
            return true;
        }
    }

    // If function has not returned by here, then input string was invalid.
    parseErr($str, 'Could not parse next token');
}

// Returns the token at a look ahead of one. (Not the current token, but the one after.)
function lookAheadOne(string $str) : string
{
    getNextTok($str, $type, $val);
    getNextTok($str, $type, $val);
    return $val;
}

/*
Checks to see if the token next in line matches either of the values or types
given. It also returns the appropriate XML parse string.
*/
function match(string &$str, $vals, $types = []) : string
{
    if (!getNextTok($str, $type, $val))
        parseErr($str, 'Unexpected end of file reached');
    if (!in_array($val, $vals) && !in_array($type, $types)){
        echo "Value:  $val\nValues:  " . print_r($vals, true);
        echo "\nType:  $type\nTypes:  " . print_r($types, true);
        parseErr($str, 'Unexpected token');
    }
    return printTerminal($type, $val);
}

/*
Checks to see if the token next in line matches either the values or types
given without advancing the input string.
*/
function matchPeek(string $str, $vals, $types = []) : bool
{
    if (getNextTok($str, $type, $val)) {
        if (in_array($val, $vals) || in_array($type, $types)) {
            return true;
        }
    }
    return false;
}

// Takes a token and returns the XML to print.
function printTerminal(string $type, string $value) : string
{
    switch ($type) {
    case 'stringConstant':
        $retString = "<stringConstant>" . trim($value, '"') . "</stringConstant>\n";
        break;
    case 'symbol':
        $symSwitch = ['<' => '&lt;', '>' => '&gt;', '"' => '&quot;', '&' => '&amp;'];
        if (array_key_exists($value, $symSwitch)) $value = $symSwitch[$value];
        $retString = "<symbol>$value</symbol>\n";
        break;
    default:
        $retString = "<$type>$value</$type>\n";
    }

    return $retString;
}

// Appends a tab character the beginning of each line of a multiline string.
function indent($str) : string
{
    return "\t" . rtrim(str_replace("\n", "\n\t", $str), "\t");
}

function parseClass(string &$str) : string
{
    $retString = "<class>\n";
    $retString .= "\t" . match($str, ['class']);
    $retString .= "\t" . match($str, [], ['identifier']);
    $retString .= "\t" . match($str, ['{']);
    while (matchPeek($str, ['static', 'field'])) {
        $retString .= indent(parseClassVarDec($str));
    }
    while (matchPeek($str, ['constructor', 'function', 'method'])) {
        $retString .= indent(parseSubroutineDec($str));
    }
    $retString .= "\t" . match($str, ['}']);
    $retString .= "</class>\n";

    return $retString;
}

function parseClassVarDec(string &$str) : string
{
    $retString = "<classVarDec>\n";
    $retString .= "\t" . match($str, ['static', 'field']);
    $retString .= "\t" . match($str, ['int', 'char', 'boolean'], ['identifier']);
    $retString .= "\t" . match($str, [], ['identifier']);
    while (matchPeek($str, [','])) {
        $retString .= "\t" . match($str, [',']);
        $retString .= "\t" . match($str, [], ['identifier']);
    }
    $retString .= "\t" . match($str, [';']);
    $retString .= "</classVarDec>\n";

    return $retString;
}

function parseSubroutineDec(string &$str) : string
{
    $retString = "<subroutineDec>\n";
    $retString .= "\t" . match($str, ['constructor', 'function', 'method']);
    $retString .= "\t" . match($str, ['int', 'char', 'boolean', 'void'], ['identifier']);
    $retString .= "\t" . match($str, [], ['identifier']);
    $retString .= "\t" . match($str, ['(']);
    $retString .= indent(parseParameterList($str));
    $retString .= "\t" . match($str, [')']);
    $retString .= indent(parseSubroutineBody($str));
    $retString .= "</subroutineDec>\n";

    return $retString;
}

function parseParameterList(string &$str) : string
{
    $retString = "<parameterList>\n";
    if (matchPeek($str, ['int', 'char', 'boolean'], ['identifier'])) {
        $retString .= "\t" . match($str, ['int', 'char', 'boolean'], ['identifier']);
        $retString .= "\t" . match($str, [], ['identifier']);
        while (matchPeek($str, [','])) {
            $retString .= "\t" . match($str, [',']);
            $retString .= "\t" . match($str, ['int', 'char', 'boolean'], ['identifier']);
            $retString .= "\t" . match($str, [], ['identifier']);
        }
    }
    $retString .= "</parameterList>\n";

    return $retString;
}

function parseSubroutineBody(string &$str) : string
{
    $retString = "<subroutineBody>\n";
    $retString .= "\t" . match($str, ['{']);
    while (matchPeek($str, ['var'])) {
        $retString .= indent(parseVarDec($str));
    }
    $retString .= indent(parseStatements($str));
    $retString .= "\t" . match($str, ['}']);
    $retString .= "</subroutineBody>\n";

    return $retString;
}

function parseVarDec(string &$str) : string
{
    $retString = "<varDec>\n";
    $retString .= "\t" . match($str, ['var']);
    $retString .= "\t" . match($str, ['int', 'char', 'boolean'], ['identifier']);
    $retString .= "\t" . match($str, [], ['identifier']);
    while (matchPeek($str, [','])) {
        $retString .= "\t" . match($str, [',']);
        $retString .= "\t" . match($str, [], ['identifier']);
    }
    $retString .= "\t" . match($str, [';']);
    $retString .= "</varDec>\n";

    return $retString;
}

function parseStatements(string &$str) : string
{
    $retString = "<statements>\n";
    while (matchPeek($str, ['let', 'if', 'while', 'do', 'return'])) {
        if (matchPeek($str, ['let']))
            $retString .= indent(parseLetStatement($str));

        else if (matchPeek($str, ['if']))
            $retString .= indent(parseIfStatement($str));

        else if (matchPeek($str, ['while']))
            $retString .= indent(parseWhileStatement($str));

        else if (matchPeek($str, ['do']))
            $retString .= indent(parseDoStatement($str));

        else if (matchPeek($str, ['return']))
            $retString .= indent(parseReturnStatement($str));
    }
    $retString .= "</statements>\n";

    return $retString;
}

function parseLetStatement(string &$str) : string
{
    $retString = "<letStatement>\n";
    $retString .= "\t" . match($str, ['let']);
    $retString .= "\t" . match($str, [], ['identifier']);
    if (matchPeek($str, ['['])) {
        $retString .= "\t" . match($str, ['[']);
        $retString .= indent(parseExpression($str));
        $retString .= "\t" . match($str, [']']);
    }
    $retString .= "\t" . match($str, ['=']);
    $retString .= indent(parseExpression($str));
    $retString .= "\t" . match($str, [';']);
    $retString .= "</letStatement>\n";

    return $retString;
}

function parseIfStatement(string &$str) : string
{
    $retString = "<ifStatement>\n";
    $retString .= "\t" . match($str, ['if']);
    $retString .= "\t" . match($str, ['(']);
    $retString .= indent(parseExpression($str));
    $retString .= "\t" . match($str, [')']);
    $retString .= "\t" . match($str, ['{']);
    $retString .= indent(parseStatements($str));
    $retString .= "\t" . match($str, ['}']);
    if (matchPeek($str, ['else'])) {
        $retString .= "\t" . match($str, ['else']);
        $retString .= "\t" . match($str, ['{']);
        $retString .= indent(parseStatements($str));
        $retString .= "\t" . match($str, ['}']);
    }
    $retString .= "</ifStatement>\n";

    return $retString;
}

function parseWhileStatement(string &$str) : string
{
    $retString = "<whileStatement>\n";
    $retString .= "\t" . match($str, ['while']);
    $retString .= "\t" . match($str, ['(']);
    $retString .= indent(parseExpression($str));
    $retString .= "\t" . match($str, [')']);
    $retString .= "\t" . match($str, ['{']);
    $retString .= indent(parseStatements($str));
    $retString .= "\t" . match($str, ['}']);
    $retString .= "</whileStatement>\n";

    return $retString;
}

function parseDoStatement(string &$str) : string
{
    $retString = "<doStatement>\n";
    $retString .= "\t" . match($str, ['do']);
    $retString .= indent(parseSubroutineCall($str));
    $retString .= "\t" . match($str, [';']);
    $retString .= "</doStatement>\n";

    return $retString;
}

function parseReturnStatement(string &$str) : string
{
    $retString = "<returnStatement>\n";
    $retString .= "\t" . match($str, ['return']);
    if (!matchPeek($str, [';'])) {
        $retString .= indent(parseExpression($str));
    }
    $retString .= "\t" . match($str, [';']);
    $retString .= "</returnStatement>\n";

    return $retString;
}

function parseExpression(string &$str) : string
{
    $retString = "<expression>\n";
    $retString .= indent(parseTerm($str));
    while (matchPeek($str, ['+', '-', '*', '/', '&', '|', '<', '>','='])) {
        $retString .= "\t" . match($str, ['+', '-', '*', '/', '&', '|', '<', '>', '=']);
        $retString .= indent(parseTerm($str));
    }
    $retString .= "</expression>\n";

    return $retString;
}

function parseTerm(string &$str) : string
{
    $retString = "<term>\n";

    if (matchPeek($str, [], ['integerConstant'])) {
        $retString .= "\t" . match($str, [], ['integerConstant']);
    }
    else if (matchPeek($str, [], ['stringConstant'])) {
        $retString .= "\t" . match($str, [], ['stringConstant']);
    }
    else if (matchPeek($str, ['true', 'false', 'null', 'this'])) {
        $retString .= "\t" . match($str, ['true', 'false', 'null', 'this']);
    }
    else if (matchPeek($str, [], ['identifier'])) {
        if (lookAheadOne($str) === '(' || lookAheadOne($str) === '.') {
            $retString .= indent(parseSubroutineCall($str));
        }
        else {
            $retString .= "\t" . match($str, [], ['identifier']);
            if (matchPeek($str, ['['])) {
                $retString .= "\t" . match($str, ['[']);
                $retString .= indent(parseExpression($str));
                $retString .= "\t" . match($str, [']']);
            }
        }
    }
    else if (matchPeek($str, ['('])) {
        $retString .= "\t" . match($str, ['(']);
        $retString .= indent(parseExpression($str));
        $retString .= "\t" . match($str, [')']);
    }
    else if (matchPeek($str, ['-', '~'])) {
        $retString .= "\t" . match($str, ['-', '~']);
        $retString .= indent(parseTerm($str));
    }
    // else { possibly need Exception thrown here }

    $retString .= "</term>\n";

    return $retString;
}

function parseSubroutineCall(string &$str) : string
{
    $retString = match($str, [], ['identifier']);
    if (matchPeek($str, ['.'])) {
        $retString .= match($str, ['.']);
        $retString .= match($str, [], ['identifier']);
    }
    $retString .= match($str, ['(']);
    $retString .= parseExpressionList($str);
    $retString .= match($str, [')']);

    return $retString;
}

function parseExpressionList(string &$str) : string
{
    $retString = "<expressionList>\n";
    if (!matchPeek($str, [')'])) {
        $retString .= indent(parseExpression($str));
        while (matchPeek($str, [','])) {
            $retString .= "\t" . match($str, [',']);
            $retString .= indent(parseExpression($str));
        }
    }
    $retString .= "</expressionList>\n";

    return $retString;
}

main();
