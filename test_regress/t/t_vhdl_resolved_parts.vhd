-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An array of a subtype a function of the design resolves, driven whole by one process and in
-- part by another: each element is resolved over the processes that drive it, so the function
-- sees two values for y(1) and one for y(2) (14.7.2, 14.7.3.2; NVC driver6).

entity t_vhdl_resolved_parts is
end entity;

architecture a of t_vhdl_resolved_parts is
  type int_vec is array (natural range <>) of integer;
  -- How many drivers, times 1000, plus what they add up to
  function counted (v : int_vec) return integer is
    variable result : integer := 0;
  begin
    for i in v'range loop
      result := result + v(i);
    end loop;
    return v'length * 1000 + result;
  end function;
  subtype rint is counted integer;
  type rint_vec is array (natural range <>) of rint;
  signal y : rint_vec(1 to 3) := (0, 0, 0);
begin
  whole : process is
  begin
    y <= (1, 2, 3);
    wait for 1 ns;
    report "y = " & integer'image(y(1)) & " " & integer'image(y(2)) & " "
      & integer'image(y(3));
    assert y = (2006, 1002, 2010) severity failure;
    wait;
  end process;
  part1 : process is
  begin
    y(1) <= 5;
    wait;
  end process;
  part3 : process is
  begin
    y(3 to 3) <= (others => 7);
    wait;
  end process;
end architecture;
