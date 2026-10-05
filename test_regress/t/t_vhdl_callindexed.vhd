-- DESCRIPTION: Verilator: VHDL call of a function whose parameters all have
-- defaults, read as the result of calling it with none, indexed
--
-- "get2(1)", with get2(x : integer := 0) returning a BIT_VECTOR, is a call
-- with x = 1 and equally get2's result indexed by 1 (4.2.2.1, 8.4); the
-- context decides. Compared with "01" it is the call, compared with '0'
-- the indexed result (nvc sem afunc).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_callindexed is
end entity;

architecture test of t_vhdl_callindexed is
  function get2(x : integer := 0) return bit_vector is
  begin
    if x = 0 then
      return "10";
    end if;
    return "01";
  end function;
begin
  process is
  begin
    -- The call, with x = 1
    assert get2(1) = "01" severity failure;
    assert "01" = get2(1) severity failure;
    -- get2 with x = 0 gives "10", indexed from 0
    assert get2(1) = '0' severity failure;
    assert '1' = get2(0) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
