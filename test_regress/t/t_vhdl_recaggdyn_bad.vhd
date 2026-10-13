-- DESCRIPTION: Verilator: VHDL record aggregate element of another length
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- An element given a value whose length only running decides, and which is
-- not the element's (NVC bounds32)

entity t_vhdl_recaggdyn_bad is
end entity;

architecture a of t_vhdl_recaggdyn_bad is
  type rec is record
    x : bit_vector;
  end record;
  signal s : rec(x(1 to 3));
  signal n : integer := 4;
  signal v : bit_vector(1 to 10) := (others => '1');
begin
  process is
  begin
    s <= (x => v(1 to n - 1));
    wait for 1 ns;
    assert s.x = "111" severity failure;
    report "three fit";
    s <= (x => v(1 to n));
    wait for 1 ns;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
