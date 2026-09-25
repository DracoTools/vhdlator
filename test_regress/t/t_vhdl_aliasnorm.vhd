-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- "alias bv_norm : bit_vector(bv'length - 1 downto 0) is bv", over a
-- parameter handed an ascending vector, sliced at ends only running
-- settles: the slice was cut for the alias's numbering from ends already
-- put onto the parameter's, and came out back to front -- or, before that,
-- not at all. Ashenden's bv_arithmetic sign extension is the shape
-- (VESTS ch_15_dlx-b, bv_arithmetic_body.vhd).
entity t_vhdl_aliasnorm is
end entity;

architecture a of t_vhdl_aliasnorm is
  function bv_sext(bv : in bit_vector; length : in natural) return bit_vector is
    alias bv_norm : bit_vector(bv'length - 1 downto 0) is bv;
    variable result : bit_vector(length - 1 downto 0) := (others => bv(bv'left));
    variable src_length : natural := bv'length;
  begin
    if src_length > length then
      src_length := length;
    end if;
    result(src_length - 1 downto 0) := bv_norm(src_length - 1 downto 0);
    return result;
  end function bv_sext;

  function parts(bv : in bit_vector) return bit_vector is
    alias bv_norm : bit_vector(bv'length - 1 downto 0) is bv;
    variable n : natural := bv'length;
    variable r : bit_vector(bv'length + 5 downto 0);
  begin
    r(bv'length + 5 downto 6) := bv_norm(n - 1 downto 0);
    r(5 downto 3) := bv_norm(n - 1 downto n - 3);
    r(2 downto 0) := bv_norm(2 downto 0);
    return r;
  end function;
begin
  process
    variable up : bit_vector(0 to 7) := "10110010";
    variable dn : bit_vector(7 downto 0) := "10110010";
  begin
    assert bv_sext(up, 12) = "111110110010" severity failure;
    assert bv_sext(up, 4) = "0010" severity failure;
    assert bv_sext(dn, 12) = "111110110010" severity failure;
    assert bv_sext(dn, 4) = "0010" severity failure;
    assert parts(up) = "10110010" & "101" & "010" severity failure;
    assert parts(dn) = "10110010" & "101" & "010" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
