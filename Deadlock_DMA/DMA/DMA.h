#pragma once

class DMA_Connection
{
public:
	static DMA_Connection* GetInstance();

public:
	VMM_HANDLE GetHandle() const;
	bool EndConnection();

private:
	static inline DMA_Connection* m_Instance = nullptr;
	VMM_HANDLE m_VMMHandle = nullptr;

	DMA_Connection();
	~DMA_Connection();
};