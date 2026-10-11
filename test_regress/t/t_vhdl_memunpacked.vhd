-- DESCRIPTION: Verilator: arrays of arrays and of records are memories
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_memunpacked_pkg is
  type slv_array is array (natural range <>) of std_logic_vector(7 downto 0);
  type rec is record
    a : std_logic_vector(3 downto 0);
    b : integer range 0 to 15;
  end record;
  type rec_array is array (natural range <>) of rec;
  function rev(v : slv_array) return slv_array;
  procedure bump(variable v : inout slv_array; k : natural);
end package;

package body t_vhdl_memunpacked_pkg is
  function rev(v : slv_array) return slv_array is
    variable r : slv_array(v'range);
  begin
    for i in v'range loop
      r(i) := v(v'high - (i - v'low));
    end loop;
    return r;
  end function;

  procedure bump(variable v : inout slv_array; k : natural) is
  begin
    for i in v'range loop
      v(i) := std_logic_vector(unsigned(v(i)) + k);
    end loop;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_memunpacked_pkg.all;

-- A memory whose depth a generic gives, in a type left open
entity t_vhdl_memunpacked_ram is
  generic (depth : positive);
  port (
    clk   : in  std_logic;
    we    : in  std_logic;
    addr  : in  natural range 0 to depth - 1;
    din   : in  std_logic_vector(7 downto 0);
    dout  : out std_logic_vector(7 downto 0);
    whole : out slv_array(0 to depth - 1)
  );
end entity;

architecture a of t_vhdl_memunpacked_ram is
  signal mem : slv_array(0 to depth - 1) := (others => (others => '0'));
begin
  process (clk)
  begin
    if rising_edge(clk) then
      if we = '1' then
        mem(addr) <= din;
      end if;
      dout <= mem(addr);
    end if;
  end process;
  whole <= mem;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_memunpacked_pkg.all;

entity t_vhdl_memunpacked is
end entity;

architecture a of t_vhdl_memunpacked is
  subtype word is bit_vector(31 downto 0);
  type memory_array is array (0 to 2**14 - 1) of word;

  signal clk   : std_logic := '0';
  signal we    : std_logic := '0';
  signal addr  : natural range 0 to 15 := 0;
  signal din   : std_logic_vector(7 downto 0) := (others => '0');
  signal dout  : std_logic_vector(7 downto 0);
  signal whole : slv_array(0 to 15);
  signal recs  : rec_array(0 to 7) := (others => (a => "0000", b => 0));
  signal small : slv_array(0 to 3) := (x"01", x"02", x"03", x"04");
  signal part  : slv_array(0 to 1);
begin
  ram : entity work.t_vhdl_memunpacked_ram
    generic map (depth => 16)
    port map (clk => clk, we => we, addr => addr, din => din, dout => dout, whole => whole);

  part <= small(1 to 2);

  process
    variable mem  : memory_array;
    variable sum  : natural;
    variable v    : slv_array(0 to 3);
    variable cat  : slv_array(0 to 5);
    variable r    : rec;

    procedure tick is
    begin
      clk <= '1';
      wait for 1 ns;
      clk <= '0';
      wait for 1 ns;
    end procedure;
  begin
    -- A large memory, written and read a word at a time
    for i in memory_array'range loop
      mem(i) := to_bitvector(std_logic_vector(to_unsigned(i * 3, 32)));
    end loop;
    sum := 0;
    for i in 0 to 99 loop
      sum := sum + to_integer(unsigned(to_stdlogicvector(mem(i * 100))));
    end loop;
    assert sum = 1485000 report "memory sum " & integer'image(sum) severity failure;
    mem(16383)(31 downto 16) := x"abcd";
    assert mem(16383) = x"abcdbffd" report "memory word" severity failure;
    assert mem(16383)(17) = '0' and mem(16383)(16) = '1' report "memory bit" severity failure;
    assert mem'length = 16384 and mem(5)'length = 32 report "memory length" severity failure;

    -- Whole-array operations on an array of vectors
    wait for 0 ns;
    assert part = (x"02", x"03") report "slice of signal" severity failure;
    v := small;
    assert v = small report "equal" severity failure;
    v(2) := x"ff";
    assert v /= small report "not equal" severity failure;
    assert v(1 to 2) = (x"02", x"ff") report "slice" severity failure;
    cat := v(0 to 1) & small(2 to 3) & part;
    assert cat = (x"01", x"02", x"03", x"04", x"02", x"03") report "concat" severity failure;
    assert rev(small) = (x"04", x"03", x"02", x"01") report "function" severity failure;
    bump(v, 1);
    assert v = (x"02", x"03", x"00", x"05") report "procedure" severity failure;
    v(1 to 2) := v(2 to 3);
    assert v = (x"02", x"00", x"05", x"05") report "slice assign" severity failure;
    v(3)(7 downto 4) := "1010";
    assert v(3) = x"a5" report "element part" severity failure;

    -- An array of records
    recs(2) <= (a => "1100", b => 9);
    recs(5).a <= "0011";
    wait for 0 ns;
    r := recs(2);
    assert r.a = "1100" and r.b = 9 report "record element" severity failure;
    assert recs(5).a = "0011" and recs(5).b = 0 report "record member" severity failure;
    assert recs(0 to 1) = recs(6 to 7) report "record slices" severity failure;

    -- A memory in an entity of its own, written through a port
    we <= '1';
    for i in 0 to 15 loop
      addr <= i;
      din <= std_logic_vector(to_unsigned(i * 16 + 1, 8));
      tick;
    end loop;
    we <= '0';
    addr <= 7;
    tick;
    assert dout = x"71" report "ram read" severity failure;
    assert whole(15) = x"f1" and whole(0) = x"01" report "ram whole" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
