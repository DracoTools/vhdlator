-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An array of integer_vector made by a function from one element repeated
-- over a range only running settles, and allocated from that call: OSVVM's
-- MemoryGenericPkg, "new MemBlockType'(InitMemoryBlockType(BlockWidth,
-- MemoryBaseWidth))" with "return MemBlockType'(0 to 2**BlockWidth - 1 =>
-- BaseU)" (9.3.3.3, 9.3.7).

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std_unsigned.all;
package t_vhdl_memblkalloc_pkg is
  type mem_t is protected
    procedure init(bw, basew : integer);
    procedure wr(a : integer; d : integer);
    impure function rd(a : integer) return integer;
  end protected;
end package;
package body t_vhdl_memblkalloc_pkg is
  type mem_t is protected body
    type MemBlockType is array (integer range <>) of integer_vector;
    type MemBlockPtrType is access MemBlockType;
    variable blk : MemBlockPtrType;
    variable baseWidth : integer;
    function InitMemoryBlockType(BlockWidth, BaseWidth : integer) return MemBlockType is
      constant BaseU : integer_vector(BaseWidth - 1 downto 0) := (others => -1);
    begin
      return MemBlockType'(0 to 2**BlockWidth - 1 => BaseU);
    end function;
    procedure init(bw, basew : integer) is
    begin
      baseWidth := basew;
      blk := new MemBlockType'(InitMemoryBlockType(bw, basew));
    end procedure;
    procedure wr(a : integer; d : integer) is
    begin
      blk(a) := (blk(a)'range => d);
    end procedure;
    impure function rd(a : integer) return integer is
    begin
      return blk(a)(0) + blk(a)'length * 1000;
    end function;
  end protected body;
end package body;

use work.t_vhdl_memblkalloc_pkg.all;
entity t_vhdl_memblkalloc is
end entity;
architecture t of t_vhdl_memblkalloc is
  shared variable mem : mem_t;
begin
  process
  begin
    mem.init(2, 3);
    mem.wr(1, 7);
    report integer'image(mem.rd(0)) & " " & integer'image(mem.rd(1));
    assert mem.rd(0) = 2999 and mem.rd(1) = 3007 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
