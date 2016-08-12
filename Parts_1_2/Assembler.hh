<?hh

// Compiles VM to Hack

function main() {
  echo "This script takes VM source and outputs the compiled Hack assembly.\n";
  $srcDir = readline('Enter the source directory:  ');
  $srcDir = getcwd().'/'.trim($srcDir); # getcwd == 'get current working directory'

  $dstFileName = readline('Enter destination filename:  ');
  $dstFileName = getcwd().'/'.trim($dstFileName);
  if (pathinfo($dstFileName, PATHINFO_EXTENSION) !== 'asm') # make sure destination has extension 'asm'
    $dstFileName .= '.asm';

  $paths = scandir($srcDir);
  array_splice($paths, 0, 2); # the first two elements in the array are: '.', '..' - these are unneeded

  $dstFile = fopen("$dstFileName", 'w'); # creating dest file

  # bootstrap code
  $bootstrap = "@261\n";
  $bootstrap .= "D=A\n";
  $bootstrap .= "@SP\n";
  $bootstrap .= "M=D\n";
  $bootstrap .= "@Sys.init\n";
  $bootstrap .= "0;JMP\n";
  fwrite($dstFile, $bootstrap);

  foreach ($paths as $key) {
    if (pathinfo($key, PATHINFO_EXTENSION) !== 'vm') # checks if the file has extension other than 'vm'. If so, skip over
      continue;

    # The global variable 'FileName' will be used to name static variables
    $GLOBALS['FileName'] = strtok($key, '.');

    echo "\nWorking on ".$key."...\n";

    $srcFile = fopen("$srcDir/$key", 'r');

    while (!feof($srcFile)) {
      $line = trim(fgets($srcFile));
      if (substr($line, 0, 2) === '//' || feof($srcFile)) { # if the line is a comment, skip it
        continue;
      } else {
        fwrite($dstFile, compile($line)); # write the compiled line to the dest file
      }
    }
    fclose($srcFile);
  }
  fclose($dstFile);
}

$FileName = '';

/*
 * This function takes a line of VM code, processes it, and outputs its equivalent Hack code.
 */
function compile(string $line): string {
  $command = explode(' ', $line, 5); # convert line into an array of literals
  switch (trim($command[0])) {
    # the 'trim' solves compatibility issues for files written in Windows

    # Arithmetic operators
    case 'add':
      $retString = binary('+');
      break;
    case 'sub':
      $retString = binary('-');
      break;
    case 'neg':
      $retString = unary('-');
      break;

    # Comparison operators
    case 'eq':
      $retString = comparison('EQ');
      break;
    case 'gt':
      $retString = comparison('GT');
      break;
    case 'lt':
      $retString = comparison('LT');
      break;

    # Bitwise operators
    case 'and':
      $retString = binary('&');
      break;
    case 'or':
      $retString = binary('|');
      break;
    case 'not':
      $retString = unary('!');
      break;

    # Stack operators
    case 'push':
      $retString = push($command[1], $command[2]);
      break;
    case 'pop':
      $retString = pop($command[1], $command[2]);
      break;

    # Program flow operators
    case 'label':
      $retString = label($command[1]);
      break;
    case 'goto':
      $retString = gotoCmd($command[1], false);
      break;
    case 'if-goto':
      $retString = ifgoto($command[1]);
      break;

    # Function calling operators
    case 'call':
      $retString = call($command[1], $command[2]);
      break;
    case 'function':
      $retString = functionCmd($command[1], $command[2]);
      break;
    case 'return':
      $retString = returnCmd();
      break;

    default:
      return '';
  }

  return $retString;
}

/*
* Outputs Hack assembly which calculates a basePointer[offset] and stores the
* result in register D.
*/
function getRAM(string $basePointer, string $offset, bool $isStaticSeg): string {
  $neg = false;
  if ($offset[0] === '-') {
    $offset = substr($offset, 1);
    $neg = true;
  }
  $retString = "@$offset\n"; # load $offset into A
  $retString .= "D=A\n";
  $retString .= "@$basePointer\n"; # load the base pointer into A
  $retString .= ($isStaticSeg)? "A=A+D\n" : (($neg)? "A=M-D\n" : "A=M+D\n");
  $retString .= "D=M\n"; # load value of basePointer[offset] into D

  return $retString;
}

/*
 * Generic 'push' function. Deals with all cases of 'push segment offset'.
 */
function push(string $segment, string $offset): string {
  if ($segment === 'constant') { # 'constant' is a special case
    return pushConstant($offset);
  }

  switch ($segment) {
    case 'argument':
      $retString = getRAM('ARG', $offset, false);
      break;
    case 'local':
      $retString = getRAM('LCL', $offset, false);
      break;
    case 'static':
      $retString = "@".$GLOBALS['FileName'].".$offset\n";
      $retString .= "D=M\n";
      break;
    case 'this':
      $retString = getRAM('THIS', $offset, false);
      break;
    case 'that':
      $retString = getRAM('THAT', $offset, false);
      break;
    case 'pointer':
      $retString = getRAM('3', $offset, true);
      break;
    case 'temp':
      $retString = getRAM('5', $offset, true);
      break;

    default:
      return '';
  }

  $retString .= pushRegD(); # push value in D (segment[offset]) onto the Stack

  return $retString;
}

/*
 * Pushes a constant value onto the Stack.
 * Warning! Changes the value of register D!
 */
function pushConstant(string $constVal): string {
  $retString = "@$constVal\n"; # A = $constVal
  $retString .= "D=A\n"; # D = $constVal
  $retString .= pushRegD(); # pushes register D onto the Stack

  return $retString;
}

/*
 * Pushes value of register D onto the Stack.
 * WARNING! Function changes the value of register A!
 */
function pushRegD(): string {
  $retString = "@SP\n";
  $retString .= "A=M\n"; # A = address of top of Stack
  $retString .= "M=D\n"; # top of Stack = D ($constVal)
  $retString .= "@SP\n";
  $retString .= "M=M+1\n"; # increment SP by 1

  return $retString;
}

/*
* Pushes the current value of a base pointer onto the Stack.
* WARNING! Function changes the value of register A!
*/
function pushBasePointer(string $base): string {
  $retString = "@$base\n";
  $retString .= "D=M\n";
  $retString .= pushRegD();

  return $retString;
}

/*
 * Pops the top of the Stack into segment[offset].
 */
function pop(string $segment, string $offset): string {
  # $loadBasePointer will be the segment base pointer
  # $loadAddress will be the method we use to calculate segment[offset]
  switch ($segment) {
    case 'argument':
      $loadBasePointer = "ARG";
      $loadAddress = "D=M+D";
      break;
    case 'local':
      $loadBasePointer = "LCL";
      $loadAddress = "D=M+D";
      break;
    case 'static':
      $retString = popToReg('D');
      $retString.="@".$GLOBALS['FileName'].".$offset\n";
      $retString.="M=D\n";
      return $retString;
    case 'this':
      $loadBasePointer = "THIS";
      $loadAddress = "D=M+D";
      break;
    case 'that':
      $loadBasePointer = "THAT";
      $loadAddress = "D=M+D";
      break;
    case 'pointer':
      $loadBasePointer = "3";
      $loadAddress = "D=A+D";
      break;
    case 'temp':
      $loadBasePointer = "5";
      $loadAddress = "D=A+D";
      break;
    default:
      return '';
  }

  $retString = "@$offset\n";
  $retString .= "D=A\n";
  $retString .= "@$loadBasePointer\n";
  $retString .= "$loadAddress\n";
  $retString .= "@R13\n";
  $retString .= "M=D\n";
  $retString .= popToReg('D');
  $retString .= "@R13\n";
  $retString .= "A=M\n";
  $retString .= "M=D\n";

  return $retString;
}

/*
 * Pops the top of the Stack into the given register.
 * WARNING! Function changes the value of register A!
 */
function popToReg(string $reg): string {
  $retString = "@SP\n";
  $retString .= "M=M-1\n";
  $retString .= "A=M\n";
  $retString .= "$reg=M\n";

  return $retString;
}

/*
* Pops the top of the Stack into the given base pointer.
* WARNING! Function changes the value of register D!
*/
function popToBasePointer($base): string {
  $retString = "@$base\n";
  $retString .= popToReg('D');
  $retString .= "M=D\n";

  return $retString;
}

/*
 * Generic unary operator. Parameter 'op' can have the values: '!' or '-'
 */
function unary(string $op): string {
  $retString = popToReg('D');
  $retString .= "D=$op"."D\n";
  $retString .= pushRegD();

  return $retString;
}

/*
 * Generic binary operator. Parameter 'op' can have the values: '+', '-', '&', or '|'.
 */
function binary(string $op): string {
  $retString = popToReg('D');
  $retString .= popToReg('A');
  $retString .= "D=A$op"."D\n";
  $retString .= pushRegD();

  return $retString;
}

/*
 * Generic comparison operator. Parameter 'op' can have the values: 'EQ', 'GT', or 'LT'
 */
function comparison(string $op): string {
  static $counter = 0;
  $retString = popToReg('D');
  $retString .= popToReg('A');
  $retString .= "D=A-D\n";
  $retString .= "@$op$counter\n";
  $retString .= "D;J$op\n";
  $retString .= "@NOT_$op$counter\n";
  $retString .= "D=0;JMP\n";
  $retString .= "($op$counter)\n";
  $retString .= "D=-1\n";
  $retString .= "(NOT_$op$counter)\n";
  $retString .= pushRegD();

  $counter++;

  return $retString;
}

# This will be appended to labels when they are compiled from VM to Hack
$CurrentFunction = '';

/*
* Compiles the VM 'label' command to Hack
*/
function label(string $label): string {
  return "(".$GLOBALS['CurrentFunction']."$$label)\n";
}

/*
* Compiles the VM 'goto' command to Hack
*/
function gotoCmd(string $label, bool $isFuncCall): string {
  $retString = "@".(($isFuncCall)? '' : $GLOBALS['CurrentFunction'].'$')."$label\n";
  $retString .= "0;JMP\n";

  return $retString;
}

/*
* Compiles the VM 'if-goto' command to Hack
*/
function ifgoto(string $label): string {
  $retString = popToReg('D');
  $retString .= "@".$GLOBALS['CurrentFunction']."$$label\n";
  $retString .= "D;JNE\n";

  return $retString;
}

/*
* Compiles the VM 'call' command to Hack. This means that it is partially
* responsible for setting up the new Stack Frame on the
* Global Stack.
*/
function call(string $func, string $numArgs): string {
  static $counter = 0;
  $retString = pushConstant("RETURN$counter");
  $retString .= pushBasePointer('LCL');
  $retString .= pushBasePointer('ARG');
  $retString .= pushBasePointer('THIS');
  $retString .= pushBasePointer('THAT');
  $retString .= "@".strval(intval($numArgs) + 5)."\n";
  $retString .= "D=A\n";
  $retString .= "@SP\n";
  $retString .= "D=M-D\n";
  $retString .= "@ARG\n";
  $retString .= "M=D\n";
  $retString .= "@SP\n";
  $retString .= "D=M\n";
  $retString .= "@LCL\n";
  $retString .= "M=D\n";
  $retString .= gotoCmd($func, true);
  $retString .= "(RETURN$counter)\n";

  $counter++;

  return $retString;
}

/*
* Compiles the VM 'function' command to Hack.
*/
function functionCmd(string $funcName, string $numLocals): string {
  $GLOBALS['CurrentFunction'] = $funcName;
  $retString = "($funcName)\n";
  $len = intval($numLocals);
  for ($i = 0; $i < $len; $i++)
    $retString .= pushConstant('0');

  return $retString;
}

/*
* Compiles the VM 'return' command to Hack. This means that it is responsible for
* returning the Stack Frame to its previous state (for the calling function).
*/
function returnCmd(): string {
  $retString = getRAM('LCL', '-5', false); # puts local[-5] into D
  $retString .= "@R14\n"; # R14 will be our temporary 'Ret' variable
  $retString .= "M=D\n"; # Ret = D
  $retString .= pop('argument', '0'); # repositioning function's return value for the caller
  $retString .= "@ARG\n";
  $retString .= "D=M+1\n";
  $retString .= "@SP\n";
  $retString .= "M=D\n"; # finished repositioning the SP for the calling function

  $pointers = ['THAT', 'THIS', 'ARG', 'LCL'];
  for ($i = 0; $i < 4; $i++) { # setting dynamic pointers to their previous values
    $retString .= getRAM('LCL', strval(-($i+1)), false);
    $retString .= "@".$pointers[$i]."\n";
    $retString .= "M=D\n";
  }

  $retString .= "@R14\n"; # jumping to address stored in 'Ret' (the return address)
  $retString .= "A=M\n";
  $retString .= "0;JMP\n";

  return $retString;
}

main();
