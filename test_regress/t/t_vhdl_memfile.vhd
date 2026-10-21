-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A package's function returning a matrix whose bounds its parameters give,
-- called with a constant the instance's generics give and the length of a
-- subtype the generics size: each instance's copy is as large as its
-- generics make it.
-- PoC's ocram fills its memory from mem_ReadMemoryFile(FilePath, DEPTH,
-- word_t'length, ...), and an empty file name never reads at all.

library ieee;
use ieee.std_logic_1164.all;
use std.textio.all;
package t_vhdl_memfile_pkg is
  type T_SLM is array (natural range <>, natural range <>) of std_logic;
  impure function read_lines(FileName : string; Lines : positive; Bits : positive)
    return T_SLM;
end package;

package body t_vhdl_memfile_pkg is
  impure function read_lines(FileName : string; Lines : positive; Bits : positive)
    return T_SLM is
    file fh : text open READ_MODE is FileName;
    variable l : line;
    variable word : std_logic_vector(Bits - 1 downto 0);
    variable Result : T_SLM(Lines - 1 downto 0, Bits - 1 downto 0);
  begin
    Result := (others => (others => 'U'));
    for i in 0 to Lines - 1 loop
      exit when endfile(fh);
      readline(fh, l);
      read(l, word);
      for j in 0 to Bits - 1 loop
        Result(i, j) := word(j);
      end loop;
    end loop;
    return Result;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use std.textio.all;
use work.t_vhdl_memfile_pkg.all;
entity t_vhdl_memfile_ram is
  generic (A_BITS : positive := 2; D_BITS : positive := 8; FILENAME : string := "");
  port (q1 : out std_logic_vector(D_BITS - 1 downto 0);
        q2 : out std_logic_vector(D_BITS - 1 downto 0));
end entity;
architecture a of t_vhdl_memfile_ram is
  constant DEPTH : positive := 2**A_BITS;
  subtype word_t is std_logic_vector(D_BITS - 1 downto 0);
  type ram_t is array (0 to DEPTH - 1) of word_t;
  impure function init(FilePath : string) return ram_t is
    variable Memory : T_SLM(DEPTH - 1 downto 0, word_t'range);
    variable res : ram_t;
  begin
    if FilePath'length = 0 then
      return (others => (others => '0'));
    end if;
    Memory := read_lines(FilePath, DEPTH, word_t'length);
    for i in Memory'range(1) loop
      for j in word_t'range loop
        res(i)(j) := Memory(i, j);
      end loop;
    end loop;
    return res;
  end function;
  function bits(n : natural) return word_t is
    variable r : word_t;
    variable v : natural := n;
  begin
    for j in r'reverse_range loop
      if v mod 2 = 1 then
        r(j) := '1';
      else
        r(j) := '0';
      end if;
      v := v / 2;
    end loop;
    return r;
  end function;
  signal mem : ram_t := init("");
begin
  process
    file fo : text;
    variable l : line;
    variable loaded : ram_t;
  begin
    q1 <= mem(1);
    q2 <= (others => 'X');
    if FILENAME'length > 0 then
      file_open(fo, FILENAME, WRITE_MODE);
      for i in 0 to DEPTH - 1 loop
        write(l, bits(i * 3 + 1));
        writeline(fo, l);
      end loop;
      file_close(fo);
      loaded := init(FILENAME);
      q2 <= loaded(DEPTH - 1);
    end if;
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_memfile is
end entity;
architecture a of t_vhdl_memfile is
  signal a1, a2 : std_logic_vector(7 downto 0);
  signal b1, b2 : std_logic_vector(5 downto 0);
begin
  ua : entity work.t_vhdl_memfile_ram port map (q1 => a1, q2 => a2);
  ub : entity work.t_vhdl_memfile_ram
    generic map (A_BITS => 3, D_BITS => 6, FILENAME => "t_vhdl_memfile.tmp")
    port map (q1 => b1, q2 => b2);
  process
  begin
    wait for 1 ns;
    assert to_string(a1) = "00000000" report "a1 " & to_string(a1) severity failure;
    assert to_string(a2) = "XXXXXXXX" report "a2 " & to_string(a2) severity failure;
    assert to_string(b1) = "000000" report "b1 " & to_string(b1) severity failure;
    -- The last of eight lines, 7 * 3 + 1
    assert to_string(b2) = "010110" report "b2 " & to_string(b2) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
