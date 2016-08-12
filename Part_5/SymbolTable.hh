<?hh //strict

class SymbolTable {
	private array<string, int> $indices;
	private array<string, array<string, mixed>> $table;

	public function __construct()
	{
		$this->indices = ['static' => 0, 'field' => 0, 'arg' => 0, 'var' => 0];
		$this->table = [];
	}

	public function define(string $name, string $type, string $kind) : void
	{
		$this->table[$name] = [
			'type' => $type,
			'kind' => $kind,
			'index' => $this->indices[$kind]
		];
		$this->indices[$kind]++;
	}

	public function kindCount(string $kind) : int
	{
		return $this->indices[$kind];
	}

	public function kindOf(string $name) : string
	{
		return (string)$this->table[$name]['kind'];
	}

	public function typeOf(string $name) : string
	{
		return (string)$this->table[$name]['type'];
	}

	public function indexOf(string $name) : int
	{
		return (int)$this->table[$name]['index'];
	}

	public function isDefined(string $name) : bool
	{
		return array_key_exists($name, $this->table);
	}
}
