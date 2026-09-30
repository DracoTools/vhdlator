-- DESCRIPTION: Verilator: VHDL record aggregate sized by elaboration, run
--
-- This file is part of Verilator.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
--
-- The same record variable as t_vhdl_elabrecagg, whose element a generic
-- sizes, given an aggregate of another width in the branch that does run.
-- The element's length is not locally static, so the mismatch is found
-- when the assignment is made (10.6.2.1): nvc 1.22.1 stops the run there
-- with "value length 8 does not match target length 16", GHDL 7.0.0-dev with
-- "bound check failure".

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_elabrecagg_bad is
  generic (g_len : positive := 16);
end entity;

architecture a of t_vhdl_elabrecagg_bad is
  type trans_t is record
    data : std_logic_vector;
    last : boolean;
  end record;
  signal tdata : std_logic_vector(g_len - 1 downto 0) := (others => '0');
begin
  process
    variable t : trans_t(data(tdata'range));
  begin
    if g_len = 16 then
      t := (data => x"99", last => true);
    end if;
    report "assigned, length " & integer'image(t.data'length);
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
