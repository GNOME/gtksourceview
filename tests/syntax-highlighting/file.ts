// Declaration
declare var v: any;

// Namespace
namespace N {}

// Modules
declare module '*.txt' {
  export const txt: string;
}

// Types
let num: number = 1;
let str: string = 'str';
let bool: boolean = true;
let obj: object = {};
let sym: symbol = Symbol();
let something: any;
let dunno: unknown;
let none: null = null;
let undef: undefined = undefined;

// Void type
function nothing(): void {
  return;
}

// Never type
function err(): never {
  throw new Error();
}

// Custom type
type T = Array<string | number>;
type Keys = keyof {a: number; b: number};
type Inferred<T> = T extends {a: infer U; b: infer U} ? U : never;

// Utility types
let part: Partial<{a: number; b: number}>;
let ro: Readonly<string[]>;
let rec: Record<string, any>;
let pick: Pick<{a: number; b: number}, 'a'>;
let omit: Omit<{a: number; b: number}, 'b'>;
let exc: Exclude<'a' | 'b' | 'c', 'a'>;
let ext: Extract<'a' | 'b' | 'c', 'a' | 'f'>;
let nonNull: NonNullable<string | null>;
let retT: ReturnType<() => string>;
let inst: InstanceType<typeof Number>;
let req: Required<{a?: number; b?: string}>;
let thisT: ThisType<any>;

// Interfaces
interface I {
  prop: T;
}
interface II extends I {
  readonly r: string;
}

// Class
abstract class C implements II {
  prop: T;
  readonly r: string;

  public pub: number;
  protected prot: number;
  private priv: number;

  constructor(private priv2: string) {}

  public abstract abst(): void;
}

// Enums
enum Dir {
  Up = 1,
  Down,
}
const enum E {
  Yes = 1,
  No = Yes - 1,
}

// Type guard
function isNumber(n: any): n is number {
  return typeof n === 'number';
}
