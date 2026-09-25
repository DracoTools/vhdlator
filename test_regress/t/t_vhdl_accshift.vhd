-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- "p.all := p.all(2 to n) & a": a value that carries its own bounds --
-- a concatenation of a slice of the object and one more element -- copied
-- into what an access value points at. The copy read its source a place at
-- a time and knew only an access value's own elements, so it was refused.
-- OSVVM's CoveragePkg keeps the last few values of a transition this way
-- (TCover), which VUnit's tb_axi_stream reaches.
entity t_vhdl_accshift is
end entity;

architecture sim of t_vhdl_accshift is
  type int_ptr is access integer_vector;
  type cov_t is record
    count : natural;
    last : int_ptr;
  end record;
  type cov_array is array (natural range <>) of cov_t;
  type cov_ptr is access cov_array;
begin
  process
    variable p : int_ptr;
    variable cov : cov_ptr;
    constant id : natural := 1;
  begin
    p := new integer_vector'(1, 2, 3, 4);
    p.all := p.all(1 to 3) & 9;
    assert p.all = (2, 3, 4, 9) severity failure;
    p.all := 0 & p.all(1 to 3);
    assert p.all = (0, 3, 4, 9) severity failure;

    cov := new cov_array(0 to 2);
    cov(id).last := new integer_vector'(5, 6, 7);
    for a in 10 to 12 loop
      cov(id).count := cov(id).count + 1;
      cov(id).last.all := cov(id).last.all(1 to 2) & a;
    end loop;
    assert cov(id).count = 3 severity failure;
    assert cov(id).last.all = (10, 11, 12) severity failure;
    assert cov(id).last'length = 3 and cov(id).last.all'left = 0 severity failure;

    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
