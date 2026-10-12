-- DESCRIPTION: Verilator: VHDL bound checks around elaboration constants
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_boundcheck_elab_rom is
  port (address : in std_logic_vector;
        data : inout std_logic_vector);
end entity;

architecture a of t_vhdl_boundcheck_elab_rom is
begin
  process is
    -- A range taken from a port's attribute, indexed while running and
    -- written through a procedure's out parameter
    subtype word is std_logic_vector(0 to data'length - 1);
    type storage_t is array (natural range 0 to 2 ** address'length - 1) of word;
    variable storage : storage_t;
    variable index : natural := 0;
    procedure fill (v : out word; k : natural) is
    begin
      v := (others => '1');
      v(k) := '0';
    end procedure;
  begin
    data <= (data'range => 'Z');
    while index < 4 loop
      fill(storage(index), index);
      index := index + 1;
    end loop;
    assert storage(2) = "11011111" severity failure;
    assert storage(3) = "11101111" severity failure;
    wait for 1 ns;
    data <= storage(to_integer(unsigned(address)));
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_boundcheck_elab is
  generic (n : natural := 3);
end entity;

architecture a of t_vhdl_boundcheck_elab is
  type nat_arr is array (natural range <>) of natural;
  function widths (k : natural) return nat_arr is
    variable r : nat_arr(0 to k - 1);
  begin
    for i in r'range loop
      r(i) := 4 + i;
    end loop;
    return r;
  end function;
  function lows (w : nat_arr) return nat_arr is
    variable r : nat_arr(w'range);
  begin
    for i in w'range loop
      if i = 0 then
        r(i) := 0;
      else
        r(i) := r(i - 1) + w(i - 1);
      end if;
    end loop;
    return r;
  end function;
  constant c_widths : nat_arr := widths(n);
  constant c_lows : nat_arr := lows(c_widths);
  signal dout : std_logic_vector(14 downto 0);
  signal address : std_logic_vector(1 downto 0) := "10";
  signal data : std_logic_vector(0 to 7);
begin
  -- A subtype bound indexed by a generate index: settled per instance
  gen : for i in 0 to n - 1 generate
    signal s : unsigned(c_widths(i) - 1 downto 0) := (others => '0');
  begin
    process is
    begin
      wait for 1 ns;
      s <= s + 1;
      wait;
    end process;
    dout(c_lows(i) + c_widths(i) - 1 downto c_lows(i)) <= std_logic_vector(s);
  end generate;

  rom : entity work.t_vhdl_boundcheck_elab_rom port map (address, data);

  process is
  begin
    wait for 2 ns;
    assert dout = "000001000010001" severity failure;
    assert data = "11011111" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
