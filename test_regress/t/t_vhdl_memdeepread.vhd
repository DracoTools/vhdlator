-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An element of an array of integer_vector reached through two access values,
-- "MemStructPtr(1).MemArrayPtr(b)(a)", the second an element of an array of
-- them dereferenced where it stands, handed to a function of an open formal:
-- OSVVM's MemoryGenericPkg MemRead (8.1, 4.2.2.1).

package t_vhdl_memdeepread_pkg is
  type mem_t is protected
    procedure init(bw, basew : integer);
    procedure wr(b, a : integer; d : integer);
    impure function rd(b, a : integer) return integer;
  end protected;
end package;
package body t_vhdl_memdeepread_pkg is
  type mem_t is protected body
    type MemBlockType is array (integer range <>) of integer_vector;
    type MemBlockPtrType is access MemBlockType;
    type MemArrayType is array (integer range <>) of MemBlockPtrType;
    type MemArrayPtrType is access MemArrayType;
    type MemStructType is record
      MemArrayPtr : MemArrayPtrType;
      BaseWidth : natural;
    end record;
    type ItemArrayType is array (integer range <>) of MemStructType;
    type ItemArrayPtrType is access ItemArrayType;
    variable MemStructPtr : ItemArrayPtrType;
    function InitMemoryBlockType(BlockWidth, BaseWidth : integer) return MemBlockType is
      constant BaseU : integer_vector(BaseWidth - 1 downto 0) := (others => -1);
    begin
      return MemBlockType'(0 to 2**BlockWidth - 1 => BaseU);
    end function;
    function sumOf(Mem : integer_vector; Size : integer) return integer is
      variable t : integer := 0;
    begin
      for i in Mem'range loop t := t + Mem(i); end loop;
      return t * 10 + Size;
    end function;
    procedure init(bw, basew : integer) is
    begin
      MemStructPtr := new ItemArrayType(1 to 1);
      MemStructPtr(1).BaseWidth := basew;
      MemStructPtr(1).MemArrayPtr := new MemArrayType(0 to 3);
      MemStructPtr(1).MemArrayPtr(2) := new MemBlockType'(InitMemoryBlockType(bw, basew));
    end procedure;
    procedure wr(b, a : integer; d : integer) is
    begin
      MemStructPtr(1).MemArrayPtr(b)(a) := (MemStructPtr(1).MemArrayPtr(b)(a)'range => d);
    end procedure;
    impure function rd(b, a : integer) return integer is
    begin
      return sumOf(MemStructPtr(1).MemArrayPtr(b)(a), 4);
    end function;
  end protected body;
end package body;

use work.t_vhdl_memdeepread_pkg.all;
entity t_vhdl_memdeepread is
end entity;
architecture t of t_vhdl_memdeepread is
  shared variable mem : mem_t;
begin
  process
  begin
    mem.init(2, 3);
    mem.wr(2, 1, 7);
    report integer'image(mem.rd(2, 0)) & " " & integer'image(mem.rd(2, 1));
    assert mem.rd(2, 0) = -26 and mem.rd(2, 1) = 214 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
