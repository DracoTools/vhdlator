-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A top-level entity with a generic package: nothing above it can give the
-- package (6.5.5). nvc and Questa refuse it too (GHDL gna issue3167).

package t_vhdl_toppkggen_pkg is
  generic (W : positive := 8);
  subtype word_t is bit_vector(W - 1 downto 0);
end package;

entity t_vhdl_toppkggen_bad is
  generic (
    W : positive := 16;
    package P is new work.t_vhdl_toppkggen_pkg generic map (W => W)
  );
  port (din : in P.word_t; dout : out P.word_t);
end entity;

architecture rtl of t_vhdl_toppkggen_bad is
begin
  dout <= din;
end architecture;
