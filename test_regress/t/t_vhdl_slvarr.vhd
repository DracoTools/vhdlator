-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- An array whose elements are arrays with no length of their own, where
-- neither the array's length nor its elements' is known before the design
-- runs: UVVM's t_slv_array, allocated as "new t_slv_array(0 to n - 1)(w - 1
-- downto 0)" from a command's fields and handed to a BFM procedure that
-- copies it into a local of its own and walks it word by word
-- (bitvis_vip_avalon_st/src/avalon_st_vvc.vhd, avalon_st_bfm_pkg.vhd).
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_slvarr is
end entity;

architecture a of t_vhdl_slvarr is
  type t_slv_array is array (natural range <>) of std_logic_vector;
  type t_slv_array_ptr is access t_slv_array;

  signal d : std_logic_vector(7 downto 0) := (others => '0');
  signal words : integer := 0;

  -- Walks what it is handed, a word at a time, as a BFM does
  procedure transmit(data_array : in t_slv_array; signal dout : out std_logic_vector) is
    constant C_WORD : natural := data_array(data_array'low)'length;
    variable v_norm : t_slv_array(0 to data_array'length - 1)(C_WORD - 1 downto 0) := data_array;
  begin
    assert v_norm'ascending and v_norm'left = 0 severity failure;
    for i in v_norm'range loop
      dout <= v_norm(i);
      wait for 1 ns;
    end loop;
  end procedure;

  function ones(data_array : t_slv_array) return natural is
    variable n : natural := 0;
  begin
    for i in data_array'range loop
      for j in data_array(i)'range loop
        if data_array(i)(j) = '1' then
          n := n + 1;
        end if;
      end loop;
    end loop;
    return n;
  end function;
begin
  process
    variable p : t_slv_array_ptr;
    variable n : natural := 3;
    variable w : natural := 8;
    type src_t is array (0 to 3) of std_logic_vector(15 downto 0);
    constant src : src_t := (x"1111", x"2222", x"3333", x"4444");
    variable fixed : t_slv_array(2 downto 0)(7 downto 0) := (x"0f", x"f0", x"ff");
    -- Words of a fixed length, as many as only running says
    variable fl : t_slv_array(0 to n - 1)(7 downto 0);
    -- Both only running says, all of it '0'
    variable z : t_slv_array(0 to n - 1)(w - 1 downto 0) := (others => (others => '0'));
    variable k : natural := 1;
  begin
    p := new t_slv_array(0 to n - 1)(w - 1 downto 0);
    assert p.all'length = 3 and p(0)'length = 8 and p(0)'right = 0 severity failure;
    for i in 0 to n - 1 loop
      p(i) := src(i)(w - 1 downto 0);
    end loop;
    assert p(2) = x"33" and p(0)'length = 8 and p(1)'left = 7 severity failure;
    assert p(1)(1) = '1' and p(1)(0) = '0' severity failure;
    p(0) := p(2);
    assert p(0) = x"33" severity failure;
    assert ones(p.all) = 4 + 2 + 4 severity failure;
    assert ones(fixed) = 4 + 4 + 8 severity failure;

    -- A slice of an element, and of a fixed array at running ends
    assert p(1)(3 downto 0) = x"2" and p(2)(7 downto 4) = x"3" severity failure;
    assert ones(fixed(k downto 0)) = 4 + 8 severity failure;
    for i in fl'range loop
      fl(i) := x"01";
    end loop;
    assert ones(fl) = 3 severity failure;
    assert z'length = 3 and z(2)'length = 8 and ones(z) = 0 severity failure;
    z(1) := x"81";
    assert ones(z) = 2 and z(1)(7) = '1' severity failure;

    transmit(p.all, d);
    assert d = x"33" severity failure;
    transmit(fixed, d);
    assert d = x"ff" severity failure;
    deallocate(p);
    -- Allocated from a value whose words have a length of their own, and
    -- from one value over a range only running settles
    p := new t_slv_array'(fixed);
    assert p(1) = x"f0" and p.all'left = 2 and p(1)'length = 8 severity failure;
    p(1)(0) := '1';
    assert p(1) = x"f1" severity failure;
    deallocate(p);
    p := new t_slv_array'(0 to n - 1 => x"a5");
    assert p.all'length = 3 and p(2) = x"a5" and ones(p.all) = 12 severity failure;
    deallocate(p);
    report "*-* All Finished *-*";
    wait;
  end process;

  process (d)
  begin
    words <= words + 1;
  end process;
end architecture;
