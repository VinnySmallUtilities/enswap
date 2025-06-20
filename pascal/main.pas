// НИЧЕГО НЕ СДЕЛАНО!!!!

// {$MODE DELPHI}
{$H+}
{S}
program HelloWorld;

uses crt, Generics.Collections;

procedure sleep(const seconds: integer); stdcall; external 'libc.so';

begin
  writeln('Hello, World!');
  sleep(25);
end.
