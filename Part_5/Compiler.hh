<?hh //decl

// This script compiles Jack to VM


include('SymbolTable.hh');

/*
	This main is in charge of iterating through all of the .jack files in a source
	directory and writing the compiled code to respective .vm files.
*/
function main()
{
	echo "This script compiles Jack to VM.\n";
	$srcDir = readline('Enter a source directory:  ');
	$srcDir = getcwd() . '/' . trim($srcDir); // getcwd == 'get current working directory'
	$paths = scandir($srcDir);
	array_splice($paths, 0, 2); // the first two elements in the array are: '.', '..' - these are unneeded
	foreach($paths as $key) {
		if (pathinfo($key, PATHINFO_EXTENSION) !== 'jack') // checks if the file has extension other than 'jack'. If so, skip over
			continue;
		echo "\nCompiling " . $key . "...\n";
		$contents = file_get_contents("$srcDir/$key");
		try {
			// We start the recursive decent from the grammar's root variable 'class'
			file_put_contents("$srcDir/" . strtok($key, '.') . 'S.vm', parseClass($contents));
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
	'symbol' => '/[{}()[\].,;+\-*\/&|<>=~]/',
	'keyword' => '/(?:class|constructor|function|method|field|static|var|int|char|boolean|void|true|false|null|this|let|do|if|else|while|return)\b/',
	'identifier' => '/[a-zA-Z_]+\w*/'
];

function err(string $errType, string $errMsg, string $str) : void
{
	throw new Exception("$errType Error:\t" . $errMsg . "\nString remaining:\n" . $str);
}


/******************************
	TOKENIZER FUNCTIONS
******************************/

/*
	This function parses the value and type of the current token.
	This function advances the compiler's focus in the input string.

	Parameters: (All parameters are passed by-reference)
	in str - The remaining code yet to be parsed. This function changes the original string object.
	out type - The type of the parsed token is placed in this variable.
	out value - The value of the parsed token is placed in this variable.

	Return Value:
	Boolean - Whether or not function succeeded in parsing next token.
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
	err('Parse', 'Could not parse next token', $str);
}

/*
	Returns the token at a look ahead of one. (Not the current token, but the one after.)
	Does not advance the compiler.
*/
function lookAheadOne(string $str) : string
{
	getNextTok($str, $type, $val);
	getNextTok($str, $type, $val);
	return $val;
}

/*
	Parses the next token out of the input and makes sure that its type or its value
	is present in the passed $valids array. Function advances the compiler.

	Return Value:
	The value of the parsed token.
*/
function match(string &$str, $valids) : string
{
	if (!getNextTok($str, $type, $val))
		err('Parse', 'Unexpected end of file reached', $str);
	if (!in_array($val, $valids) && !in_array($type, $valids)){
		err('Parse', 'Unexpected token type or value', $str);
	}

	return $val;
}

/*
	Checks to see if the token next in line matches any of the values or types
	passed in the $valids array. Does not advance the compiler.
*/
function matchPeek(string $str, $valids) : bool
{
	if (getNextTok($str, $type, $val)) {
		if (in_array($val, $valids) || in_array($type, $valids)) {
			return true;
		}
	}
	return false;
}


/******************************
	COMPILER FUNCTIONS
******************************/

// Prints VM code for accessing a segment[index].
function segIndex(string $kind, int $index) : string
{
	$translate = ['field' => 'this', 'arg' => 'argument', 'var' => 'local'];
	$retString = ((array_key_exists($kind, $translate))?  $translate[$kind] : $kind)
		. " $index\n";
	return $retString;
}

function push(string $kind, int $index) : string
{
	return "push " . segIndex($kind, $index);
}

function pop(string $kind, int $index) : string
{
	return "pop " . segIndex($kind, $index);
}

/*
	Returns the 'kind' of a symbol by first looking it up in the current
	subroutine symbol table, and then if not there in the class symbol table.
*/
function kind(string $name, SymbolTable $subTable) : string
{
	if ($subTable->isDefined($name))
		return $subTable->kindOf($name);
	if ($GLOBALS['classSymbTable']->isDefined($name))
		return $GLOBALS['classSymbTable']->kindOf($name);

	err('Compile', 'Encountered undefined variable', $str);
}

// Similar to kind but returns symbol index.
function index(string $name, SymbolTable $subTable) : int
{
	if ($subTable->isDefined($name))
		return $subTable->indexOf($name);
	if ($GLOBALS['classSymbTable']->isDefined($name))
		return $GLOBALS['classSymbTable']->indexOf($name);

	err('Compile', 'Encountered undefined variable', $str);
}

// Similar to kind but returns symbol type.
function type(string $name, SymbolTable $subTable) : string
{
	if ($subTable->isDefined($name))
		return $subTable->typeOf($name);
	if ($GLOBALS['classSymbTable']->isDefined($name))
		return $GLOBALS['classSymbTable']->typeOf($name);

	err('Compile', 'Encountered undefined variable', $str);
}

// Globally defined
$classSymbTable = null;
$className = '';

// Starts the parsing and compiling of the input from the root variable 'class'
function parseClass(string &$str) : string
{
	$GLOBALS['classSymbTable'] = new SymbolTable(); // Instantiating the class (global) symbol table
	match($str, ['class']);
	$GLOBALS['className'] = match($str, ['identifier']);
	match($str, ['{']);
	while (matchPeek($str, ['static', 'field'])) {
		parseClassVarDec($str);
	}
	$retString = '';
	while (matchPeek($str, ['constructor', 'function', 'method'])) {
		$retString .= parseSubroutineDec($str);
	}
	match($str, ['}']);

	return $retString;
}

/*
	This function returns a void because its sole purpose is to
	add to the class symbol table.
*/
function parseClassVarDec(string &$str) : void
{
	$kind = match($str, ['static', 'field']);
	$type = match($str, ['int', 'char', 'boolean', 'identifier']);
	$name = match($str, ['identifier']);
	$GLOBALS['classSymbTable']->define($name, $type, $kind);
	while (matchPeek($str, [','])) {
		match($str, [',']);
		$name = match($str, ['identifier']);
		$GLOBALS['classSymbTable']->define($name, $type, $kind);
	}
	match($str, [';']);
}

function parseSubroutineDec(string &$str) : string
{
	$subTable = new SymbolTable(); // Instantiating this subroutine's symbol table
	$subType = match($str, ['constructor', 'function', 'method']);
	if ($subType === 'method')
		$subTable->define('this', $GLOBALS['className'], 'arg');
	match($str, ['int', 'char', 'boolean', 'void', 'identifier']);
	$subName = match($str, ['identifier']);
	match($str, ['(']);
	parseParameterList($str, $subTable);
	match($str, [')']);

	$retBody = '';
	if ($subType === 'method') {
		$retBody .= push('arg', 0);
		$retBody .= pop('pointer', 0);
	}
	else if ($subType === 'constructor') {
		$retBody .= push('constant', $GLOBALS['classSymbTable']->kindCount('field'));
		$retBody .= "call Memory.alloc 1\n";
		$retBody .= pop('pointer', 0);
	}
	$retBody .= parseSubroutineBody($str, $subTable);

	$retString = 'function ' . $GLOBALS['className'] . ".$subName "
		. $subTable->kindCount('var') . "\n$retBody";

	return $retString;
}

// Similar to parseClassVarDec in that it only adds to a symbol table.
function parseParameterList(string &$str, SymbolTable $subTable) : void
{
	if (matchPeek($str, ['int', 'char', 'boolean', 'identifier'])) {
		$type = match($str, ['int', 'char', 'boolean', 'identifier']);
		$name = match($str, ['identifier']);
		$subTable->define($name, $type, 'arg');
		while (matchPeek($str, [','])) {
			match($str, [',']);
			$type = match($str, ['int', 'char', 'boolean', 'identifier']);
			$name = match($str, ['identifier']);
			$subTable->define($name, $type, 'arg');
		}
	}
}

function parseSubroutineBody(string &$str, SymbolTable $subTable) : string
{
	match($str, ['{']);
	while (matchPeek($str, ['var'])) {
		parseVarDec($str, $subTable);
	}
	$retString = parseStatements($str, $subTable);
	match($str, ['}']);

	return $retString;
}

// Only adds to subroutine's symbol table.
function parseVarDec(string &$str, SymbolTable $subTable) : void
{
	match($str, ['var']);
	$type = match($str, ['int', 'char', 'boolean', 'identifier']);
	$name = match($str, ['identifier']);
	$subTable->define($name, $type, 'var');
	while (matchPeek($str, [','])) {
		match($str, [',']);
		$name = match($str, ['identifier']);
		$subTable->define($name, $type, 'var');
	}
	match($str, [';']);
}

function parseStatements(string &$str, SymbolTable $subTable) : string
{
	$retString = '';
	while (matchPeek($str, ['let', 'if', 'while', 'do', 'return'])) {
		if (matchPeek($str, ['let']))
			$retString .= parseLetStatement($str, $subTable);

		else if (matchPeek($str, ['if']))
			$retString .= parseIfStatement($str, $subTable);

		else if (matchPeek($str, ['while']))
			$retString .= parseWhileStatement($str, $subTable);

		else if (matchPeek($str, ['do']))
			$retString .= parseDoStatement($str, $subTable);

		else if (matchPeek($str, ['return']))
			$retString .= parseReturnStatement($str, $subTable);
	}

	return $retString;
}

function parseLetStatement(string &$str, SymbolTable $subTable) : string
{
	$isArr = false;
	$retString = '';

	match($str, ['let']);
	$destVar = match($str, ['identifier']);
	if (matchPeek($str, ['['])) {
		$retString .= push(kind($destVar, $subTable), index($destVar, $subTable));
		match($str, ['[']);
		$retString .= parseExpression($str, $subTable);
		match($str, [']']);
		$retString .= "add\n";
		$isArr = true;
	}
	match($str, ['=']);
	$retString .= parseExpression($str, $subTable);
	match($str, [';']);
	if ($isArr) {
		$retString .= pop('temp', 0);
		$retString .= pop('pointer', 1);
		$retString .= push('temp', 0);
		$retString .= pop('that', 0);
	}
	else
		$retString .= pop(kind($destVar, $subTable), index($destVar, $subTable));

	return $retString;
}

// IDEA: Maybe reset 'if' and 'while' counters for every class.
function parseIfStatement(string &$str, SymbolTable $subTable) : string
{
	static $ifCounter = 0;
	$currCounter = $ifCounter++;

	match($str, ['if']);
	match($str, ['(']);
	$retString = parseExpression($str, $subTable);
	match($str, [')']);
	$retString .= "if-goto IF_TRUE$currCounter\n";
	$retString .= "goto IF_FALSE$currCounter\n";
	match($str, ['{']);
	$retString .= "label IF_TRUE$currCounter\n";
	$retString .= parseStatements($str, $subTable);
	match($str, ['}']);
	if (matchPeek($str, ['else'])) {
		$retString .= "goto IF_END$currCounter\n";
		match($str, ['else']);
		match($str, ['{']);
		$retString .= "label IF_FALSE$currCounter\n";
		$retString .= parseStatements($str, $subTable);
		match($str, ['}']);
		$retString .= "label IF_END$currCounter\n";
	}
	else
		$retString .= "label IF_FALSE$currCounter\n";

	return $retString;
}

function parseWhileStatement(string &$str, SymbolTable $subTable) : string
{
	static $whileCounter = 0;
	$currCounter = $whileCounter++;
	match($str, ['while']);
	$retString = "label WHILE_EXP$currCounter\n";
	match($str, ['(']);
	$retString .= parseExpression($str, $subTable);
	match($str, [')']);
	$retString .= "not\n";
	match($str, ['{']);
	$retString .= "if-goto WHILE_END$currCounter\n";
	$retString .= parseStatements($str, $subTable);
	match($str, ['}']);
	$retString .= "goto WHILE_EXP$currCounter\n";
	$retString .= "label WHILE_END$currCounter\n";

	return $retString;
}

function parseDoStatement(string &$str, SymbolTable $subTable) : string
{
	match($str, ['do']);
	$retString = parseSubroutineCall($str, $subTable);
	match($str, [';']);
	$retString .= pop('temp', 0);

	return $retString;
}

function parseReturnStatement(string &$str, SymbolTable $subTable) : string
{
	match($str, ['return']);
	if (!matchPeek($str, [';'])) {
		$retString = parseExpression($str, $subTable);
	}
	else {
		$retString = push('constant', 0);
	}
	match($str, [';']);
	$retString .= "return\n";

	return $retString;
}

function parseExpression(string &$str, SymbolTable $subTable) : string
{
	$retString = parseTerm($str, $subTable);
	while (matchPeek($str, ['+', '-', '*', '/', '&', '|', '<', '>','='])) {
		$op = match($str, ['+', '-', '*', '/', '&', '|', '<', '>', '=']);
		$retString .= parseTerm($str, $subTable);
		switch($op) {
		case '+':
			$retString .= "add\n";
			break;
		case '-':
			$retString .= "sub\n";
			break;
		case '*':
			$retString .= "call Math.multiply 2\n";
			break;
		case '/':
			$retString .= "call Math.divide 2\n";
			break;
		case '&':
			$retString .= "and\n";
			break;
		case '|':
			$retString .= "or\n";
			break;
		case '<':
			$retString .= "lt\n";
			break;
		case '>':
			$retString .= "gt\n";
			break;
		case '=':
			$retString .= "eq\n";
			break;
		}
	}

	return $retString;
}

function parseTerm(string &$str, SymbolTable $subTable) : string
{
	if (matchPeek($str, ['integerConstant'])) {
		$constVal = match($str, ['integerConstant']);
		$retString = push('constant', intval($constVal));
	}
	else if (matchPeek($str, ['stringConstant'])) {
		$strVal = trim(match($str, ['stringConstant']), '"');
		$retString = push('constant', strlen($strVal));
		$retString .= "call String.new 1\n";
		for ($i = 0; $i < strlen($strVal); $i++) {
			$retString .= push('constant', ord($strVal[$i]));
			$retString .= "call String.appendChar 2\n";
		}
	}
	else if (matchPeek($str, ['true', 'false', 'null', 'this'])) {
		$boolVal = match($str, ['true', 'false', 'null', 'this']);
		switch($boolVal) {
		case 'false': // FALLTHROUGH - (for Hacklang compiler)
		case 'null':
			$retString = push('constant', 0);
			break;
		case 'true':
			$retString = push('constant', 0);
			$retString .= "not\n";
			break;
		case 'this':
			$retString = push('pointer', 0);
		}
	}
	else if (matchPeek($str, ['identifier'])) {
		if (lookAheadOne($str) === '(' || lookAheadOne($str) === '.') {
			$retString = parseSubroutineCall($str, $subTable);
		}
		else {
			$name = match($str, ['identifier']);
			if (matchPeek($str, ['['])) {
				match($str, ['[']);
				$retString = parseExpression($str, $subTable);
				match($str, [']']);
				$retString .= push(kind($name, $subTable), index($name, $subTable));
				$retString .= "add\n";
				$retString .= pop('pointer', 1);
				$retString .= push('that', 0);
			}
			else {
				$retString = push(kind($name, $subTable), index($name, $subTable));
			}
		}
	}
	else if (matchPeek($str, ['('])) {
		match($str, ['(']);
		$retString = parseExpression($str, $subTable);
		match($str, [')']);
	}
	else if (matchPeek($str, ['-', '~'])) {
		$unaryOp = match($str, ['-', '~']);
		$retString = parseTerm($str, $subTable);
		if ($unaryOp === '-')
			$retString .= "neg\n";
		else if ($unaryOp === '~')
			$retString .= "not\n";
	}
	// else { possibly need Exception thrown here }

	return $retString;
}

function parseSubroutineCall(string &$str, SymbolTable $subTable) : string
{
	$retString = '';
	$numArgs = 0;

	$className = match($str, ['identifier']);
	if (matchPeek($str, ['.'])) {
		match($str, ['.']);
		$subName = match($str, ['identifier']);
	}
	else {
		$subName = $className;
		$className = $GLOBALS['className'];
		$retString .= push('pointer', 0);
		$numArgs++;
	}

	if ($subTable->isDefined($className) || $GLOBALS['classSymbTable']->isDefined($className)) {
		$retString .= push(kind($className, $subTable), index($className, $subTable));
		$className = type($className, $subTable);
		$numArgs++;
	}

	match($str, ['(']);
	$retString .= parseExpressionList($str, $subTable, $numArgs);
	match($str, [')']);
	$retString .= "call $className.$subName $numArgs\n";

	return $retString;
}

function parseExpressionList(string &$str, SymbolTable $subTable, int &$numArgs) : string
{
	$retString = '';
	if (!matchPeek($str, [')'])) {
		$retString .= parseExpression($str, $subTable);
		$numArgs++;
		while (matchPeek($str, [','])) {
			match($str, [',']);
			$retString .= parseExpression($str, $subTable);
			$numArgs++;
		}
	}

	return $retString;
}

main();
