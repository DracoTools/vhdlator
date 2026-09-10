-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An alternative of a conditional assignment or an if statement that a
-- constant condition closes off never runs, so the length check that happens when an
-- assignment runs never applies to it: nvc's bounds/issue806 writes one
-- value per possible width and lets a constant pick.

entity t_vhdl_deadalt is
end entity;

architecture a of t_vhdl_deadalt is
  constant W : natural := 9;
  signal size_8 : bit_vector(7 downto 0) := x"a5";
  signal size_9 : bit_vector(8 downto 0) := '1' & x"5a";
  signal o1 : bit_vector(W - 1 downto 0);
begin

  o1 <= size_9 when W = 9 else size_8;

  process
    variable v : bit_vector(W - 1 downto 0);
  begin
    v := size_8 when W = 8 else size_9;
    if W = 9 then
      v := size_9;
    else
      v := size_8;
    end if;
    wait for 1 ns;
    assert o1 = '1' & x"5a" report "wrong signal value" severity failure;
    assert v = '1' & x"5a" report "wrong variable value" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
