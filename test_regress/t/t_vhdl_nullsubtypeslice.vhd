-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Fields packed into one vector by subtypes stacked on each other's
-- attributes and the ports' lengths, "din(id_r) <= m_id" with "subtype id_r
-- is natural range s_id'length + resp_r'high downto resp_r'high + 1": in an
-- instance without an id the slice is null and the assignment does nothing
-- (5.2.1). Written as a part select the other way round, it overwrote the
-- field below with what a null value is held as. Shaped after open-logic's
-- olo_axi_pl_stage write response channel.

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_nullsubtypeslice_m is
  generic (idw : natural := 0; uw : natural := 0);
  port (m_id : in std_logic_vector(idw - 1 downto 0); m_resp : in std_logic_vector(1 downto 0);
        m_user : in std_logic_vector(uw - 1 downto 0);
        s_id : out std_logic_vector(idw - 1 downto 0); s_resp : out std_logic_vector(1 downto 0);
        s_user : out std_logic_vector(uw - 1 downto 0));
end entity;
architecture a of t_vhdl_nullsubtypeslice_m is
begin
  b : block is
    subtype resp_r is natural range s_resp'length - 1 downto 0;
    subtype id_r is natural range s_id'length + resp_r'high downto resp_r'high + 1;
    subtype user_r is natural range s_user'length + id_r'high downto id_r'high + 1;
    signal din, dout : std_logic_vector(user_r'high downto 0);
  begin
    din(id_r) <= m_id;
    din(resp_r) <= m_resp;
    din(user_r) <= m_user;
    dout <= din;
    s_id <= dout(id_r);
    s_resp <= dout(resp_r);
    s_user <= dout(user_r);
  end block;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_nullsubtypeslice is
end entity;
architecture a of t_vhdl_nullsubtypeslice is
  signal s_resp : std_logic_vector(1 downto 0);
  signal s_id0 : std_logic_vector(-1 downto 0);
  signal nul : std_logic_vector(-1 downto 0);
  signal s_user0 : std_logic_vector(-1 downto 0);
  signal s_resp4 : std_logic_vector(1 downto 0);
  signal s_id4 : std_logic_vector(3 downto 0);
  signal s_user4 : std_logic_vector(1 downto 0);
begin
  u0 : entity work.t_vhdl_nullsubtypeslice_m port map (m_id => nul, m_resp => "10", m_user => nul, s_id => s_id0,
                               s_resp => s_resp, s_user => s_user0);
  u4 : entity work.t_vhdl_nullsubtypeslice_m generic map (idw => 4, uw => 2)
    port map (m_id => "1001", m_resp => "01", m_user => "11", s_id => s_id4, s_resp => s_resp4,
              s_user => s_user4);
  process
  begin
    wait for 1 ns;
    assert s_resp = "10" report "resp0 " & to_string(s_resp) severity failure;
    assert s_resp4 = "01" and s_id4 = "1001" and s_user4 = "11"
      report "u4 " & to_string(s_resp4) & to_string(s_id4) & to_string(s_user4) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
