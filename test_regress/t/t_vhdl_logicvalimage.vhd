-- DESCRIPTION: Verilator: VHDL 'IMAGE of 'VAL for a logic type
--
-- "t'image(t'val(n))" is the name of the literal at position n (16.2), for
-- every one of the nine values of STD_ULOGIC.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_logicvalimage is
end entity;

architecture test of t_vhdl_logicvalimage is
  function is_std_logic (constant c : character) return boolean is
  begin
    for i in 0 to 8 loop
      if c = std_logic'image(std_logic'val(i))(2) then
        return true;
      end if;
    end loop;
    return false;
  end function;
begin
  process
    constant all9 : string := "UX01ZWLH-";
    variable n : natural := 6;
  begin
    for k in all9'range loop
      assert is_std_logic(all9(k)) report "not found: " & all9(k) severity failure;
    end loop;
    assert not is_std_logic('A') severity failure;
    assert std_logic'image(std_logic'val(n)) = "'L'" severity failure;
    assert std_ulogic'image(std_ulogic'val(8)) = "'-'" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
